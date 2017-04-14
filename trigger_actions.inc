<?php

/**
 * @file
 * This supplements the actions functions provided in Backdrop core.
 */


/**
 * Performs a given list of actions by executing their callback functions.
 *
 * Given the IDs of actions to perform, this function finds out what the
 * callback functions for the actions are by querying the database. Then
 * it calls each callback using the function call $function($object, $context,
 * $a1, $a2), passing the input arguments of this function (see below) to the
 * action function.
 *
 * @param $action_ids
 *   The IDs of the actions to perform. Can be a single action ID or an array
 *   of IDs. IDs of configurable actions must be given as numeric action IDs;
 *   IDs of non-configurable actions may be given as action function names.
 * @param $object
 *   The object that the action will act on: a node, user, or comment object.
 * @param $context
 *   Associative array containing extra information about what triggered
 *   the action call, with $context['hook'] giving the name of the hook
 *   that resulted in this call to actions_do().
 * @param $a1
 *   Passed along to the callback.
 * @param $a2
 *   Passed along to the callback.
 *
 * @return
 *   An associative array containing the results of the functions that
 *   perform the actions, keyed on action ID.
 *
 * @ingroup actions
 */
function trigger_actions_do($action_ids, $object = NULL, $context = NULL, $a1 = NULL, $a2 = NULL) {
  // $stack tracks the number of recursive calls.
  static $stack;
  $stack++;
  if ($stack > config_get('trigger_actions.settings', 'actions_max_stack')) {
    watchdog('actions', 'Stack overflow: too many calls to actions_do(). Aborting to prevent infinite recursion.', array(), WATCHDOG_ERROR);
    return;
  }
  $actions = array();
  $available_actions = trigger_actions_get_all_actions();
  $actions_result = array();
  if (is_array($action_ids)) {
    $conditions = array();
    foreach ($action_ids as $action_id) {
      if (is_numeric($action_id)) {
        $conditions[] = $action_id;
      }
      elseif (isset($available_actions[$action_id])) {
        $actions[$action_id] = $available_actions[$action_id];
      }
    }

    // When we have action instances we must go to the database to retrieve
    // instance data.
    if (!empty($conditions)) {
      $query = db_select('trigger_actions');
      $query->addField('actions', 'aid');
      $query->addField('actions', 'type');
      $query->addField('actions', 'callback');
      $query->addField('actions', 'node_type');
      $query->addField('actions', 'node_ID');
      $query->addField('actions', 'parameters');
      $query->condition('aid', $conditions, 'IN');
      $result = $query->execute();
      foreach ($result as $action) {
        $actions[$action->aid] = $action->parameters ? unserialize($action->parameters) : array();
        $actions[$action->aid]['callback'] = $action->callback;
        $actions[$action->aid]['type'] = $action->type;
      }
    }

    // Fire actions, in no particular order.
    foreach ($actions as $action_id => $params) {
      // Configurable actions need parameters.
      if (is_numeric($action_id)) {
        $function = $params['callback'];
        if (function_exists($function)) {
          $context = array_merge($context, $params);
          $actions_result[$action_id] = $function($object, $context, $a1, $a2);
        }
        else {
          $actions_result[$action_id] = FALSE;
        }
      }
      // Simple action; $action_id is the function name.
      else {
        $actions_result[$action_id] = $action_id($object, $context, $a1, $a2);
      }
    }
  }
  // Optimized execution of a single action.
  else {
    // If it's a configurable action, retrieve stored parameters.
    if (is_numeric($action_ids)) {
      $action = db_query("SELECT callback, parameters FROM {trigger_actions} WHERE aid = :aid", array(':aid' => $action_ids))->fetchObject();
      $function = $action->callback;
      if (function_exists($function)) {
        $context = array_merge($context, unserialize($action->parameters));
        $actions_result[$action_ids] = $function($object, $context, $a1, $a2);
      }
      else {
        $actions_result[$action_ids] = FALSE;
      }
    }
    // Singleton action; $action_ids is the function name.
    else {
      if (function_exists($action_ids)) {
        $actions_result[$action_ids] = $action_ids($object, $context, $a1, $a2);
      }
      else {
        // Set to avoid undefined index error messages later.
        $actions_result[$action_ids] = FALSE;
      }
    }
  }
  $stack--;
  return $actions_result;
}


/**
 * Retrieves all action instances from the database.
 *
 * This function differs from the actions_get_info() function, which gathers
 * actions by invoking hook_action_info(). The actions returned by this
 * function and the actions returned by actions_get_info() are partially
 * synchronized. Non-configurable actions from hook_action_info()
 * implementations are put into the database when trigger_actions_synchronize()
 * is called, which happens when admin/config/system/actions is visited.
 * Configurable actions are not added to the database until they are configured
 * in the user interface, in which case a database row is created for each
 * configuration of each action.
 *
 * @return
 *   Associative array keyed by numeric action ID. Each value is an associative
 *   array with keys 'callback', 'label', 'type' and 'configurable'.
 */
function trigger_actions_get_all_actions() {
  $actions = db_query("SELECT * FROM {trigger_actions}")->fetchAllAssoc('aid', PDO::FETCH_ASSOC);
  foreach ($actions as &$action) {
    // $action['configurable'] = (bool) $action['parameters'];
    unset($action['parameters']);
    unset($action['aid']);
  }
  return $actions;
}


/**
 * Creates an associative array keyed by hashes of function names or IDs.
 *
 * Hashes are used to prevent actual function names from going out into HTML
 * forms and coming back.
 *
 * @param $actions
 *   An associative array with function names or action IDs as keys
 *   and associative arrays with keys 'label', 'type', etc. as values.
 *   This is usually the output of config_get('trigger_actions.settings',
 *   'available_actions') or actions_get_info().
 *
 * @return
 *   An associative array whose keys are hashes of the input array keys, and
 *   whose corresponding values are associative arrays with components
 *   'callback', 'label', 'type', and 'configurable' from the input array.
 */
function trigger_actions_actions_map($actions) {
  $actions_map = array();
  foreach ($actions as $callback => $array) {
    $key = backdrop_hash_base64($callback);
    $actions_map[$key]['callback'] = isset($array['callback']) ? $array['callback'] : $callback;
    $actions_map[$key]['label'] = $array['label'];
    $actions_map[$key]['type'] = $array['type'];
    $actions_map[$key]['configurable'] = $array['configurable'];
    $actions_map[$key]['trigger_names'] = $array['trigger_names'];
  }
  return $actions_map;
}

/**
 * Returns an action array key (function or ID), given its hash.
 *
 * Faster than trigger_actions_actions_map() when you only need
 * the function name or ID.
 *
 * @param $hash
 *   Hash of a function name or action ID array key. The array key
 *   is a key into the return value of actions_get_info() (array key is the action
 *   function name) or trigger_actions_get_all_actions() (array key is the action ID).
 *
 * @return
 *   The corresponding array key, or FALSE if no match is found.
 */
function trigger_actions_function_lookup($hash) {
  // Check for a function name match.
  $actions_list = actions_get_info();
  foreach ($actions_list as $function => $array) {
    if (backdrop_hash_base64($function) == $hash) {
      return $function;
    }
  }
  $aid = FALSE;
  // Must be a configurable action; check database.
  $result = db_query("SELECT aid FROM {trigger_actions} WHERE parameters <> ''")->fetchAll(PDO::FETCH_ASSOC);
  foreach ($result as $row) {
    if (backdrop_hash_base64($row['aid']) == $hash) {
      $aid = $row['aid'];
      break;
    }
  }
  return $aid;
}

/**
 * Synchronizes actions that are provided by modules in hook_action_info().
 *
 * Actions provided by modules in hook_action_info() implementations are
 * synchronized with actions that are stored in the trigger_actions
 * database table. This is necessary so that actions that do not require
 * configuration can receive action IDs.
 *
 * @param $delete_orphans
 *   If TRUE, any actions that exist in the database but are no longer
 *   found in the code (for example, because the module that provides them has
 *   been disabled) will be deleted.
 */
function trigger_actions_synchronize($delete_orphans = FALSE) {
  $actions_in_code = actions_get_info();
  $actions_in_db = db_query("SELECT aid, callback, label FROM {trigger_actions} ")->fetchAllAssoc('aid', PDO::FETCH_ASSOC);
  // Go through all the actions provided by modules.
  // Function actions_get_info() returns a field 'callback'
  // which gives the name of the function that performs the action
  // if different from the name of the action.
  // If it is a configurable action there will also be a function that
  // creates a form and this will be called <function name>_form.

  foreach ($actions_in_code as $action => $array) {
    // if already in database we can eliminate this action
    if (isset($actions_in_db[$action])) {
      unset($actions_in_db[$action]);
    }
    else {
      // This is an action without a record in the database.
      // Check whether this action is configurable and if it is, make sure that
      // this is recorded in database.
      $action_function = isset($array['callback']) ? $array['callback'] : $action;
      $function = $action_function . '_form';
      // If a custom configuration form exists,
      // set configurable to value 2
      // is there a settings for the triggers for this action
      // if so, copy it to the database too.
      $this_module = $array['module'];
      if (($this_module == 'node') OR ($this_module == 'comment') OR ($this_module == 'user') OR ($this_module == 'system') OR ($this_module == 'path')) {
        $this_module = 'trigger_actions';
      }
      $triggers = config_get("$this_module.settings", "triggers.$action");
      if (isset($triggers)) {
        $trigger_names = ' ';
        foreach ($triggers as $this_trigger => $value) {
          if ($value == 1) {
            $trigger_names = $trigger_names . ' ' . $this_trigger;
          }
        }
      }
      $is_configurable = function_exists($function) ? 2 : 0;
      $file = isset($array['file']) ? $array['file'] : '';
      db_insert('trigger_actions')
        ->fields(array(
          'aid' => $action,
          'type' => $array['type'],
          'callback' => $action_function,
          'parameters' => '',
          'label' => $array['label'],
          'configurable' => $is_configurable,
          'trigger_names' => $trigger_names,
          'file' => $file,
        ))
        ->execute();
      watchdog('trigger_actions', "Action '%action' added.", array('%action' => $action));
    }
  }


  // Any actions that we have left in $actions_in_db are orphaned.
  if ($actions_in_db) {
    $orphaned = array_keys($actions_in_db);

    if ($delete_orphans) {
      $actions = db_query('SELECT aid, label FROM {trigger_actions} WHERE callback IN (:orphaned)', array(':orphaned' => $orphaned))->fetchAll();
      foreach ($actions as $action) {
        trigger_actions_delete($action->aid);
        watchdog('trigger_actions', "Removed orphaned action '%action' from database.", array('%action' => $action->label));
      }
    }
    else {
      $link = l(t('Remove orphaned actions'), 'admin/config/trigger_actions/actions/orphan');
      $count = count($actions_in_db);
      $orphans = implode(', ', $orphaned);
      watchdog('trigger_actions', '@count orphaned actions (%orphans) exist in the actions table. !link', array(
        '@count' => $count,
        '%orphans' => $orphans,
        '!link' => $link
      ), WATCHDOG_INFO);
    }
  }
}


/**
 * Saves an action and its user-supplied parameter values to the database.
 *
 * @param $function
 *   The name of the function to be called when this action is performed.
 * @param $type
 *   The type of action, to describe grouping and/or context, e.g., 'node',
 *   'user', 'comment', or 'system'.
 * @param $params
 *   An associative array with parameter names as keys and parameter values as
 *   values.
 * @param $label
 *   A user-supplied label of this particular action, e.g., 'Send email
 *   to Jim'.
 * @param $configurable
 *   Set to 2 for configurable actions with custom settings, 1 for advanced actions, 0 otherwise.
 * @param $node_type
 *   An optional user-supplied value for this particular action, restricting
 *   its application to a specified node type.
 * @param $node_id
 *   An optional user-supplied value for this particular action, restricting
 *   its application to a specified node ID.
 * @param $aid
 *   The ID of this action. If omitted, a new action is created.
 *
 * @return
 *   The ID of the action.
 */
function trigger_actions_save($function, $type, $params, $label, $configurable, $node_type = '', $node_id = 0, $trigger_names = '', $aid = NULL) {
  // For simple actions (non-configurable) the callback is used as the aid.
  // For advanced (configurable) actions we assign a number (integer).

  if (!$aid) {
    $aid = db_next_id();
  }

  db_merge('trigger_actions')
    ->key(array('aid' => $aid))
    ->fields(array(
      'callback' => $function,
      'type' => $type,
      'parameters' => serialize($params),
      'label' => $label,
      'configurable' => $configurable,
      'node_type' => $node_type,
      'node_ID' => $node_id,
      'trigger_names' => $trigger_names,
    ))
    ->execute();

  watchdog('trigger_actions', 'Action %action saved.', array('%action' => $label));
  return $aid;
}

/**
 * Retrieves a single action from the database.
 *
 * @param $aid
 *   The ID of the action to retrieve.
 *
 * @return
 *   The appropriate action row from the database as an object.
 */
function trigger_actions_load($aid) {
  return db_query("SELECT * FROM {trigger_actions} WHERE aid = :aid", array(':aid' => $aid))->fetchObject();
}

/**
 * Deletes a single action from the database.
 *
 * @param $aid
 *   The ID of the action to delete.
 */
function trigger_actions_delete($aid) {
  db_delete('trigger_actions')
    ->condition('aid', $aid)
    ->execute();
  module_invoke_all('trigger_actions_delete', $aid);
}

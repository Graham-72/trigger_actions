<?php

/**
 * @file
 * Admin page callbacks for the trigger module.
 */

/**
 * Builds the form that allows users to assign actions to triggers.
 *
 * @param $module_to_display
 *   Which tab of triggers to display. E.g., 'node' for all
 *   node-related triggers.
 * @return
 *   HTML form.
 */
function trigger_actions_assign($module_to_display = NULL) {
  // If no type is specified we default to node actions, since they
  // are the most common.
  if (!isset($module_to_display)) {
    backdrop_goto('admin/structure/trigger_actions/node');
  }
  // build list of triggers for this type of node from trigger_action_info hooks
  $actions_in_code = actions_get_info();
  module_load_include('inc', 'trigger_actions');
  trigger_actions_synchronize();
  $build = array();
  $trigger_info = module_invoke_all('trigger_actions_info');
  backdrop_alter('trigger_actions_info', $trigger_info);
  foreach ($trigger_info as $module => $info) {
    if ($module == $module_to_display) {
      foreach ($info as $trigger_name => $description) {
        // create a form for each relevant trigger
        $form_id = 'trigger_actions_' . $trigger_name . '_assign_form';
        $build[$form_id] = backdrop_get_form($form_id, $module, $trigger_name, $description['label']);
      }
    }
  }
  return $build;
}

/**
 * Confirm removal of an assigned action.
 *
 * @param $module
 *   The tab of triggers the user will be directed to after successful
 *   removal of the action, or if the confirmation form is cancelled.
 * @param $trigger_name
 * @param $aid
 *   The action ID.
 * @ingroup forms
 * @see trigger_actions_unassign_submit()
 */
function trigger_actions_unassign($form, $form_state, $module = NULL, $trigger_name = NULL, $aid = NULL) {
  module_load_include('inc', 'trigger_actions');
  if (!($trigger_name && $aid)) {
    backdrop_goto('admin/structure/trigger_actions');
  }
  $form['trigger_name'] = array(
    '#type' => 'value',
    '#value' => $trigger_name,
  );
  $form['module'] = array(
    '#type' => 'value',
    '#value' => $module,
  );
  $form['aid'] = array(
    '#type' => 'value',
    '#value' => $aid,
  );

  $action = trigger_actions_function_lookup($aid);
  $actions = trigger_actions_get_all_actions();

  $destination = 'admin/structure/trigger_actions/' . $module;

  return confirm_form($form,
    t('Are you sure you want to unassign the action %title?', array('%title' => $actions[$action]['label'])),
    $destination,
    t('You can assign it again later if you wish.'),
    t('Unassign'), t('Cancel')
  );
}

/**
 * Submit callback for trigger_actions_unassign() form.
 */
function trigger_actions_unassign_submit($form, &$form_state) {
  if ($form_state['values']['confirm'] == 1) {
    $aid = trigger_actions_function_lookup($form_state['values']['aid']);
    db_delete('trigger_actions_assignments')
      ->condition('trigger_name', $form_state['values']['trigger_name'])
      ->condition('aid', $aid)
      ->execute();
    backdrop_static_reset('trigger_get_assigned_actions');
    $actions = trigger_actions_get_all_actions();
    watchdog('trigger_actions', 'Action %action has been unassigned.', array('%action' => $actions[$aid]['label']));
    backdrop_set_message(t('Action %action has been unassigned.', array('%action' => $actions[$aid]['label'])));
    $form_state['redirect'] = 'admin/structure/actions/assignments';
  }
  else {
    backdrop_goto('admin/structure/trigger_actions');
  }
}

/**
 * Returns the form for assigning an action to a trigger.
 *
 * @param $module
 *   The name of the trigger group, e.g., 'node'.
 * @param $trigger_name
 *   The name of the trigger, e.g., 'node_insert'.
 * @param $label
 *   A plain English description of what this trigger does.
 *
 * @ingoup forms
 * @see trigger_actions_assign_form_validate()
 * @see trigger_actions_assign_form_submit()
 */
function trigger_actions_assign_form($form, $form_state, $module, $trigger_name, $label) {
  module_load_include('inc', 'trigger_actions');
  $form['module'] = array(
    '#type' => 'hidden',
    '#value' => $module,
  );
  $form['trigger_name'] = array(
    '#type' => 'hidden',
    '#value' => $trigger_name,
  );
  // All of these forms use the same validate and submit functions.
  $form['#validate'][] = 'trigger_actions_assign_form_validate';
  $form['#submit'][] = 'trigger_actions_assign_form_submit';


  $options = array();
  $functions = array();
  $these_actions = array();
  // Restrict the options list to actions that declare support for this trigger.
  $all_actions = trigger_actions_get_all_actions();
  foreach ($all_actions as $action => $data) {
    if (isset($data['trigger_names'])) {
      // get the actions relevant to this $trigger_name
      $trigger_names = $data['trigger_names'];
      $is_any = FALSE;
      $is_trigger = FALSE;
      // check for 'any'
      if (strpos($trigger_names, 'any') !== FALSE) {
        $is_any = TRUE;
      }
      // check for value of this 'trigger_name'
      if (strpos($trigger_names, $trigger_name) !== FALSE) {
        $is_trigger = TRUE;
      }
      // check for either 'any' or 'trigger_name' value
      if ($is_any || $is_trigger) {
        $these_actions[$action] = $data;
      }
    }
  }
  // $these_actions is an array listing all actions that specify this trigger
  foreach ($these_actions as $aid => $action) {
    $options[$action['type']][$aid] = $action['label'];
  }
  $form[$trigger_name] = array(
    '#type' => 'fieldset',
    // !description is correct, since these labels are passed through t() in
    // hook_trigger_actions_info().
    '#title' => t('Trigger: !description', array('!description' => $label)),
    '#theme' => 'trigger_actions_display',
  );

  // Retrieve actions that are already assigned to this trigger combination.
  $actions = trigger_actions_get_assigned_actions($trigger_name);
  $form[$trigger_name]['assigned']['#type'] = 'value';
  $form[$trigger_name]['assigned']['#value'] = array();
  foreach ($actions as $aid => $info) {
    // If action is defined offer to unassign it,
    // otherwise offer to delete all orphaned actions.
    $hash = backdrop_hash_base64($aid, TRUE);
    if (trigger_actions_function_lookup($hash)) {
      $form[$trigger_name]['assigned']['#value'][$aid] = array(
        'label' => $info['label'],
        'link' => l(t('unassign'), "admin/structure/trigger_actions/unassign/$module/$trigger_name/$hash"),
      );
    }
    else {
      // Link to system_actions_remove_orphans() to do the clean up.
      $form[$trigger_name]['assigned']['#value'][$aid] = array(
        'label' => $info['label'],
        'link' => l(t('Remove orphaned actions'), "admin/structure/actions/orphan"),
      );
    }
  }
  $form[$trigger_name]['parent'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('container-inline')),
  );
  // List possible actions that may be assigned.
  if (count($options) != 0) {
    $form[$trigger_name]['parent']['aid'] = array(
      '#type' => 'select',
      '#title' => t('List of trigger actions when !description', array('!description' => $label)),
      '#title_display' => 'invisible',
      '#options' => $options,
      '#empty_option' => t('Choose an action'),
    );
    $form[$trigger_name]['parent']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Assign')
    );
  }
  else {
    $form[$trigger_name]['none'] = array(
      '#markup' => t('No actions available for this trigger. <a href="@link">Add action</a>.', array('@link' => url('admin/config/trigger_actions/actions/manage')))
    );
  }
  return $form;
}

/**
 * Validation function for trigger_actions_assign_form().
 *
 * Makes sure that the user is not re-assigning an action to an event.
 */
function trigger_actions_assign_form_validate($form, $form_state) {
  module_load_include('inc', 'trigger_actions');
  $form_values = $form_state['values'];
  $aid = $form_values['aid'];
  if (!empty($form_values['aid'])) {
    $aid_exists = db_query("SELECT aid FROM {trigger_actions_assignments} WHERE (trigger_name = :trigger_name AND aid = :aid)", array(
      ':trigger_name' => $form_values['trigger_name'],
      ':aid' => $aid,
    ))->fetchField();

    if ($aid_exists) {
      form_set_error($form_values['trigger_name'], t('The action you chose is already assigned to that trigger.'));
    }
  }
}

/**
 * Submit function for trigger_actions_assign_form().
 */
function trigger_actions_assign_form_submit($form, &$form_state) {
  if (!empty($form_state['values']['aid'])) {
    $aid = $form_state['values']['aid'];
    $trigger_name = $form_state['values']['trigger_name'];
    $weight = db_query("SELECT MAX(weight) FROM {trigger_actions_assignments} WHERE trigger_name = :trigger_name", array(':trigger_name' => $trigger_name))->fetchField();

    // Insert the new action.
    db_insert('trigger_actions_assignments')
      ->fields(array(
        'trigger_name' => $form_state['values']['trigger_name'],
        'aid' => $aid,
        'weight' => $weight + 1,
      ))
      ->execute();

    // If we are not configuring an action for a "presave" trigger and this action
    // changes an object property, then we need to save the object, so the
    // property change will persist.
    $actions = trigger_actions_get_all_actions();
    if (strpos($form_state['values']['trigger_name'], 'presave') === FALSE && isset($actions[$aid]['behavior']) && in_array('changes_property', $actions[$aid]['behavior'])) {
      // Determine the corresponding save action name for this action.
      $save_action = strtok($aid, '_') . '_save_action';
      // If no corresponding save action exists, we need to bail out.
      if (!isset($actions[$save_action])) {
        throw new Exception(t('Missing/undefined save action (%save_aid) for %aid action.', array(
          '%save_aid' => $aid,
          '%aid' => $aid
        )));
      }
      // Delete previous save action if it exists, and re-add it using a higher
      // weight.
      $save_action_assigned = db_query("SELECT aid FROM {trigger_actions_assignments} WHERE trigger_name = :trigger_name AND aid = :aid", array(
        ':trigger_name' => $form_state['values']['trigger_name'],
        ':aid' => $save_action
      ))->fetchField();

      if ($save_action_assigned) {
        db_delete('trigger_actions_assignments')
          ->condition('trigger_name', $form_state['values']['trigger_name'])
          ->condition('aid', $save_action)
          ->execute();
      }
      db_insert('trigger_actions_assignments')
        ->fields(array(
          'trigger_name' => $form_state['values']['trigger_name'],
          'aid' => $save_action,
          'weight' => $weight + 2,
        ))
        ->execute();

      // If no save action existed before, inform the user about it.
      if (!$save_action_assigned) {
        backdrop_set_message(t('The %label action has been appended, which is required to save the property change.', array('%label' => $actions[$save_action]['label'])));
      }
      // Otherwise, just inform about the new weight.
      else {
        backdrop_set_message(t('The %label action was moved to save the property change.', array('%label' => $actions[$save_action]['label'])));
      }
    }
  }
  backdrop_static_reset('trigger_actions_get_assigned_actions');
}

/************* FUNCTIONS FOR ACTIONS ADMIN **************************/

/**
 * Menu callback; Displays an overview of available and configurable actions.
 * Parameter $type = simple or advanced
 */
function trigger_actions_lists($type) {
  module_load_include('inc', 'trigger_actions');
  trigger_actions_synchronize();
  $actions = trigger_actions_get_all_actions();
  $actions_map = trigger_actions_actions_map($actions);
  $options = array();
  foreach ($actions_map as $key => $array) {
    if ($array['configurable']) {
      $options[$key] = $array['label'] . '...';
    }
  }
  if ($type == 'simple') {
    $result = array();
    $key = array();
    foreach ($actions as $action => $data) {
      if (!$data['configurable']) {
        $result[$action] = $data;
        $key[$action] = backdrop_hash_base64($action);
      }
    }
    $header = array(
      array('data' => t('Action type'), 'field' => 'type'),
      array('data' => t('Label'), 'field' => 'label'),
      array('data' => t('Possible Triggers'), 'field' => 'trigger_names'),
      array('data' => t('Edit Triggers'), 'field' => 'edit_triggers'),
    );
    $row = array();
    foreach ($result as $action => $data) {
      $row[] = array(
        array('data' => $data['type']),
        array('data' => check_plain($data['label'])),
        array('data' => $data['trigger_names']),
        array('data' => l(t('edit'), "admin/structure/actions/simple/edit/$key[$action]")),
      );
    }
    if ($row) {
      $pager = theme('pager');
      if (!empty($pager)) {
        $row[] = array(array('data' => $pager, 'colspan' => '3'));
      }
      $build['trigger_actions_header'] = array('#markup' => '<h3>' . t('Simple actions:') . '</h3><p>' . t('These simple actions may be edited and made configurable if needed.') . '</p>');
      $build['trigger_actions_table'] = array(
        '#markup' => theme('table', array(
          'header' => $header,
          'rows' => $row
        ))
      );
    }
    return $build;
  }

  if ($type == 'advanced') {
    $result = array();
    $key = array();
    foreach ($actions as $action => $data) {
      if (!is_numeric($action) && ($data['configurable'] > 0)) {
        $result[$action] = $data;
        $key[$action] = backdrop_hash_base64($action);
      }
    }
    $header = array(
      array('data' => t('Action type'), 'field' => 'type'),
      array('data' => t('Label'), 'field' => 'label'),
      array('data' => t('Possible Triggers'), 'field' => 'trigger_names'),
      array('data' => t('Operations'), 'field' => 'operations'),
    );

    foreach ($result as $action => $data) {
      $row[] = array(
        array('data' => $data['type']),
        array('data' => check_plain($data['label'])),
        array('data' => $data['trigger_names']),
        array('data' => l(t('add new variant'), "admin/structure/actions/advanced/configure/$key[$action]")),
      );
    }
    if ($row) {
      $pager = theme('pager');
      if (!empty($pager)) {
        $row[] = array(array('data' => $pager, 'colspan' => '3'));
      }
      $build['trigger_actions_header'] = array('#markup' => '<h3>' . t('Configurable actions:') . '</h3><p>' . t('These actions have configuration settings and may be configured to create an Advanced Action.') . '<br/>' .t('Before they can be assigned to a trigger they need to be configured by creating a new variant as an advanced action.') . '</p>' );
      $build['trigger_actions_table'] = array(
        '#markup' => theme('table', array(
          'header' => $header,
          'rows' => $row
        ))
      );
    }

    return $build;
  }
}

/**
 * Menu callback; Displays a list of configured advanced actions.
 */
function trigger_actions_configure() {
  module_load_include('inc', 'trigger_actions');
  $actions = array();
  $build = array();
  $row = array();
  $result = db_query("SELECT * FROM {trigger_actions} WHERE configurable > 0 AND parameters <> ''")->fetchAll(PDO::FETCH_ASSOC);
  foreach ($result as $data) {
    $actions[$data['aid']] = $data;
  }
  $header = array(
    array('data' => t('Action type'), 'field' => 'type'),
    array('data' => t('Label'), 'field' => 'label'),
    array('data' => t('Possible Triggers'), 'field' => 'trigger_names'),
    array('data' => t('Operations'), 'colspan' => '2'),
  );
  foreach ($result as $action => $data) {
    $action = backdrop_hash_base64($data['aid']);
    $label = check_plain($data['label']);
    $row[] = array(
      array('data' => $data['type']),
      array('data' => $label),
      array('data' => $data['trigger_names']),
      array('data' => l(t('edit'), "admin/structure/actions/advanced/configure/$action")),
      array('data' => l(t('delete'), "admin/structure/actions/remove/$label/$action")),
    );
  }
  if ($row) {
    $pager = theme('pager');
    if (!empty($pager)) {
      $row[] = array(array('data' => $pager, 'colspan' => '3'));
    }
    $build['trigger_actions_header'] = array('#markup' => '<h3>' . t('Configured & Advanced Actions:') . '</h3><p>' . t('This page lists actions that have been configured and may be assigned to triggers.') . '<br/>' . t('To assign an action to a trigger go to the Triggers admin pages.') . '</p>');
    $build['trigger_actions_table'] = array(
      '#markup' => theme('table', array(
        'header' => $header,
        'rows' => $row
      ))
    );
  }
  else {
    $build['trigger_actions_header'] = array('#markup' => '<h3>' . t('Advanced actions:') . '</h3><p>' . t('This page lists any actions that have been configured and may be assigned to triggers.') . '</p>');
  }
  return $build;
}

/**
 * Menu callback; Creates the form for configuration of a simple action.
 *
 * @param $action
 *   Hash of an action ID or an integer. If it is a hash, we are
 *   creating a new instance. If it is an integer, we are editing an existing
 *   instance.
 * @return
 *   A form definition.
 *
 * @see trigger_actions_simple_configure_validate()
 * @see trigger_actions_simple_configure_submit()
 */
function trigger_actions_simple_configure($form, &$form_state, $action = NULL) {
  if ($action === NULL) {
    backdrop_goto('admin/structure/actions/simple');
  }
  module_load_include('inc', 'trigger_actions');
  $function = trigger_actions_function_lookup($action);
  // get data for this action
  $result = trigger_actions_load($function);
  $data = (array) $result;
  $form['actions_action'] = array(
    '#type' => 'hidden',
    '#value' => $action,
  );
  $form['actions_type'] = array(
    '#type' => 'hidden',
    '#value' => $data['type'],
  );
  $form['actions_params'] = array(
    '#type' => 'hidden',
    '#value' => isset($data['params']) ? $data['params'] : '',
  );
  $form['actions_label'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#default_value' => $data['label'],
    '#maxlength' => '255',
    '#description' => t('The label for this simple action.'),
    '#weight' => -15
  );
  $form['actions_configurable'] = array(
    '#type' => 'checkbox',
    '#title' => t('Configurable'),
    '#default_value' => $data['configurable'],
    '#description' => t('Tick to change this action to be advanced and configurable.'),
    '#weight' => -14
  );
  $form['actions_triggers'] = array(
    '#type' => 'textarea',
    '#title' => t('Triggers'),
    '#default_value' => $data['trigger_names'],
    '#description' => t("A string containing the name of the Triggers that may use this action . Separate names by space or line . Use 'any' if all may be used . "),
    '#weight' => -11
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 13
  );

  return $form;
}

/**
 * Process trigger_actions_simple_configure() form submissions.
 */
function trigger_actions_simple_configure_submit($form, &$form_state) {
  $action_hash = $form_state['values']['actions_action'];
  $action = trigger_actions_function_lookup($action_hash);
  $configurable = $form_state['values']['actions_configurable'];
  $trigger_names = $form_state['values']['actions_triggers'];
  $type = $form_state['values']['actions_type'];
  $params = $form_state['values']['actions_params'];
  $label = $form_state['values']['actions_label'];
  $aid = $action;
  trigger_actions_save($action, $type, $params, $label, $configurable, '', 0, $trigger_names, $aid);

  backdrop_set_message(t('The action has been successfully revised.'));

  $form_state['redirect'] = 'admin/structure/actions/simple/edit';
}


/**
 * Menu callback; Creates the form for configuration of an advanced action.
 *
 *
 * @param $action
 *   Hash of an action ID or an integer. If it is a hash, we are
 *   creating a new instance. If it is an integer, we are editing an existing
 *   instance.
 * @return
 *   A form definition.
 * @see trigger_actions_advanced_configure_validate()
 * @see trigger_actions_advanced_configure_submit()
 */

function trigger_actions_advanced_configure($form, &$form_state, $action) {
  module_load_include('inc', 'trigger_actions');
  if ($action === NULL) {
    backdrop_goto('admin/structure/actions/advanced');
  }
  $actions = trigger_actions_get_all_actions();
  $actions_map = trigger_actions_actions_map($actions);
  $edit = array();
  $trigger_settings = array();
  $action = trigger_actions_function_lookup($action);
  // Numeric action denotes saved instance of a configurable action.
  $default_nid = 0;
  $node_default = '';
  if (is_numeric($action)) {
    $aid = $action;
    // Load stored parameter values from database.
    $data = db_query("SELECT * FROM {trigger_actions} WHERE aid = :aid", array(':aid' => $aid))->fetch();
    $edit['actions_label'] = $data->label;
    $edit['actions_type'] = $data->type;
    $edit['actions_triggers'] = isset($data->trigger_names) ? $data->trigger_names : ' ';
    $edit['actions_nodetype'] = isset($data->node_type) ? $data->node_type : 0;
    $default_nid = isset($data->node_ID) ? $data->node_ID : 0;
    $node_default = $edit['actions_nodetype'];
    $configurable = $data->configurable;

    $trigger_settings = explode(' ', $edit['actions_triggers']);
    array_unshift($trigger_settings, ' ');
    $trigger_settings = array_flip($trigger_settings);
    $defaults = array_keys($trigger_settings);

    $function = $data->callback;
    $action = backdrop_hash_base64($data->callback);
    $params = unserialize($data->parameters);
    if ($params) {
      foreach ($params as $name => $val) {
        $edit[$name] = $val;
      }
    }
  }
  // Otherwise, we are creating a new action instance.
  else {
    $action = backdrop_hash_base64($action);
    $function = $actions_map[$action]['callback'];
    $edit['actions_label'] = $actions_map[$action]['label'];
    $edit['actions_type'] = $actions_map[$action]['type'];
    $configurable = $actions_map[$action]['configurable'];

    $trigger_settings = explode(' ', $actions_map[$action]['trigger_names']);
    array_unshift($trigger_settings, ' ');
    $trigger_settings = array_flip($trigger_settings);
    $defaults = array_keys($trigger_settings);
  }
  // get information about all possible triggers
  $trigger_info = _trigger_actions_get_all_info();
  $options = array();
  $options['any'] = 'any';
  foreach ($trigger_info as $module => $data) {
    foreach ($data as $item => $info) {
      $options[$item] = strtoupper($module) . ' - ' . $item . ' - ' . $info['label'];
    }
  }
  $node_options = node_type_get_names();

  $form['actions_label'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#default_value' => $edit['actions_label'],
    '#maxlength' => '255',
    '#description' => t('A unique label for this advanced action. This label will be displayed in the interface of modules that integrate with actions, such as Trigger module.'),
    '#weight' => -10,
  );
  $form['actions_triggers'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Select here the possible triggers for this action. <br/>Note: assigning an action to a trigger is a separate additional step.'),
    '#options' => $options,
    '#default_value' => $defaults,
  );
  $form['actions_nodetype'] = array(
    '#type' => 'radios',
    '#title' => t('Optional - Select the node type for this action.'),
    '#options' => $node_options,
    '#default_value' => $node_default,
    '#description' => t('This action can be limited to apply to only one type of node.')
  );
  $form['actions_nid'] = array(
    '#type' => 'textfield',
    '#title' => t('Optional - Node ID'),
    '#default_value' => $default_nid,
    '#maxlength' => '10',
    '#description' => t('Enter the ID of a node if this action is only to be triggered by one specific node.'),
  );
  $form['help'] = array(
    '#type' => 'markup',
    '#markup' => t('This form is for the administration of actions. It is used for identifying which triggers the action may be assigned to, and other settings if needed.'),
    '#weight' => -20,
  );
  $form['actions_configurable'] = array(
    '#type' => 'hidden',
    '#value' => $configurable,
  );
  // If this action has custom settings, merge them here.
  if ($configurable == 2) {
    $action_form = $function . '_form';
    $form = array_merge($form, $action_form($edit));
  }
  $form['actions_type'] = array(
    '#type' => 'value',
    '#value' => $edit['actions_type'],
  );
  $form['actions_action'] = array(
    '#type' => 'hidden',
    '#value' => $action,
  );
  // $aid is set when configuring an existing action instance.
  if (isset($aid)) {
    $form['actions_aid'] = array(
      '#type' => 'hidden',
      '#value' => $aid,
    );
  }
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 13
  );
  return $form;
}


/**
 * Validate system_actions_configure() form submissions.
 */
function trigger_actions_advanced_configure_validate($form, &$form_state) {
  $function = trigger_actions_function_lookup($form_state['values']['actions_action']) . '_validate';
  // Hand off validation to the action.
  if (function_exists($function)) {
    $function($form, $form_state);
  }
}


/**
 * Process trigger_actions_advanced_configure() form submissions.
 */
function trigger_actions_advanced_configure_submit($form, &$form_state) {
  $function = trigger_actions_function_lookup($form_state['values']['actions_action']);
  $submit_function = $function . '_submit';
  // Action will return keyed array of values to store.
  $params = $submit_function($form, $form_state);
  $aid = isset($form_state['values']['actions_aid']) ? $form_state['values']['actions_aid'] : NULL;
  $trigger_options = '';
  foreach ($form_state['values']['actions_triggers'] as $trigger_name => $label) {
    if (strlen($label) > 2) {
      $trigger_options = $trigger_options . $trigger_name . ' ';
    }
  }
  $actions_nid = $form_state['values']['actions_nid'] > 0 ? $form_state['values']['actions_nid'] : 0;
  trigger_actions_save($function, $form_state['values']['actions_type'], $params, $form_state['values']['actions_label'], $form_state['values']['actions_configurable'], $form_state['values']['actions_nodetype'], $actions_nid, $trigger_options, $aid);
  backdrop_set_message(t('The action has been successfully saved.'));

  $form_state['redirect'] = 'admin/structure/actions/advanced';
}

/**
 * Create the form for confirmation of deleting an action.
 *
 * @see trigger_actions_remove_form_submit()
 * @ingroup forms
 */
function trigger_actions_remove($form, &$form_state, $label, $action) {
  module_load_include('inc', 'trigger_actions');
  $action = trigger_actions_function_lookup($action);
  $form['aid'] = array(
    '#type' => 'hidden',
    '#value' => $action,
  );
  return confirm_form($form,
    t('Are you sure you want to delete the action %label?', array('%label' => $label)),
    'admin/structure/actions/manage',
    t('This cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Process trigger_actions_remove form submissions.
 *
 * Post-deletion operations for action deletion.
 */
function trigger_actions_remove_submit($form, &$form_state) {
  $aid = $form_state['values']['aid'];
  $label = $form_state['values']['label'];
  module_load_include('inc', 'trigger_actions');
  $action = trigger_actions_load($aid);
  trigger_actions_delete($aid);
  watchdog('user', 'Deleted action %aid (%action)', array(
    '%aid' => $aid,
    '%action' => $label,
  ));
  backdrop_set_message(t('Action %action was deleted', array('%action' => $label)));
  $form_state['redirect'] = 'admin/structure/actions/advanced';
}

/**
 * Post-deletion operations for deleting action orphans.
 *
 * @param $orphaned
 *   An array of orphaned actions.
 */
function trigger_action_delete_orphans_post($orphaned) {
  foreach ($orphaned as $callback) {
    backdrop_set_message(t("Deleted orphaned action(%action).", array('%action' => $callback)));
  }
}

/**
 * Remove actions that are in the database but not supported by any enabled module.
 */
function trigger_actions_remove_orphans() {
  trigger_actions_synchronize();
  backdrop_goto('admin/structure/actions/manage');
}

/**
 * Create a table listing the assignment of actions to triggers.
 */
function trigger_actions_assignments() {
  module_load_include('inc', 'trigger_actions');
  $actions = array();
  $row = array();
  $result = db_query("SELECT ta.aid, ta.trigger_name, a.type, a.label FROM {trigger_actions_assignments} ta LEFT JOIN {trigger_actions} a ON ta.aid = a.aid")->fetchAll(PDO::FETCH_ASSOC);
  foreach ($result as $data) {
    $actions[$data['aid']] = $data;
  }
  $header = array(
    array('data' => t('Trigger Name'), 'field' => 'trigger_name'),
    array('data' => t('Action ID'), 'field' => 'aid'),
    array('data' => t('Action Name'), 'field' => 'label'),
    array('data' => t('Operation'), 'field' => 'operation'),
  );
  foreach ($result as $action => $data) {
    $action_id = $data['aid'];
    $trigger_name = $data['trigger_name'];
    $action_name = $data['label'];
    $module = $data['type'];
    $hash = backdrop_hash_base64($action_id, TRUE);
    $row[] = array(
      array('data' => $trigger_name),
      array('data' => $action_id),
      array('data' => $action_name),
      array('data' => l(t('unassign'), "admin/structure/trigger_actions/unassign/$module/$trigger_name/$hash"),)
    );
  }
  if ($row) {
    $pager = theme('pager');
    if (!empty($pager)) {
      $row[] = array(array('data' => $pager, 'colspan' => '3'));
    }
    $build['trigger_actions_header'] = array('#markup' => '<h3>' . t('Current Assignments:') . '</h3>');
    $build['trigger_actions_table'] = array(
      '#markup' => theme('table', array(
        'header' => $header,
        'rows' => $row
      ))
    );
  }
  else {
    $build['trigger_actions_header'] = array('#markup' => '<h3>' . t('Current Assignments:') . '</h3><p>' . t('This page lists any assignments of actions to triggers.') . '</p>');
  }
  return $build;
}

/******************************************************/



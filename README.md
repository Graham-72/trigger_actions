# Trigger Actions

This contributed module Trigger Actions for Backdrop CMS is 
intended to be a successor to the Triggers module in Drupal 7,
adapting it to work with the Actions provided in Backdrop CMS
Core, and enhancing it to allow some conditionality, for example 
configuring an action to be applied only to one type of node.

## Introduction and Status

The Trigger Actions module provides the ability to cause actions 
to run when certain triggers take place on your site. Triggers 
are events, such as new content being added to your site or a user 
logging in, and actions are tasks, such as unpublishing content 
or emailing an administrator.

This is currently a prototype for evaluation and it does not
presently include a means for migrating from a Drupal 7 installation 
of Triggers because of differences in data tables, features and the 
Actions API. Because of these differences in the Actions API, some of
the functionality from Actions in Drupal has had to be included in
this module.

It has been implemented here as a single contributed module named
triggers_actions. It creates and uses two data tables,
triggers_actions and trigger_actions_assignments.

## Help

Triggers are events on your site, such as new content being added or 
a user logging in. The trigger_actions module associates these triggers
with actions (functional tasks), such as unpublishing content 
containing certain keywords or emailing an administrator. 

The Actions settings pages contain lists of existing actions and 
provides the ability to create and configure advanced actions 
(actions requiring configuration, such as an e-mail address or 
a list of banned words).

To set up a trigger/action combination, first visit the Actions 
configuration pages, where you can either verify that the action 
you want is already listed or you can create a new advanced action. 

You will need to set up an advanced action if there are configuration 
options in your trigger/action combination, such as specifying an 
email address for a Send Email action. After configuring or 
verifying your action, visit the Triggers configuration page and 
choose the appropriate tab (Comment, Taxonomy, etc.), where you can
assign the action to run when the trigger event occurs.



## Installation

- Install this module using the official Backdrop CMS instructions at
  https://backdropcms.org/guide/modules.

- There are two entries to the administrative pages:
  For Actions: admin/structure/actions/
  For Triggers: admin/structure/trigger_actions/
  
  See below for guidance on how these should be used.
  
## Use

The module defines an initial set of actions and also triggers. Other
modules may define more by using the hooks hook_action_info and
hook_trigger_actions_info. Modules providing actions can also define
the triggers that can be associated with an action by providing in 
their config file some settings for 'triggers'. Otherwise when 
initially installed the module has no information about which triggers
can be assigned to which actions, and this information has to be added 
for each action that is needed, using the administration pages for actions.

There can be four types of action:
  + Simple Actions have no configurable features other than the 
    triggers that can be applied to each of them.
  + Simple Actions may be adapted to be Configurable Actions 
    and can then be configured to limit their assignment
    to a particular type of node, or even a specific node,
    thereby creating Configured Actions.
  + Advanced Actions have further appropriate settings provided by
    their module. For example, the Send Email action included with
    this module has fields to specify the address the email is to be
    sent to, the title of the email and the message.
    
The administrative pages for Actions enable these configurations to
be set up and administered. There are separate pages listing the 
Simple Actions, Configurable Actions and Advanced Actions.

For example, the module can be used to send a pre-defined email to a
specified address when triggered by a system action such as addition
of new content of a particular type. This is achieved by creating a
customised 'advanced action' by configuring a copy of the 'send email'
action included in this module.

In Drupal 7 hook_action_info provides a value 'triggers' which is an 
array of the events (that is, hooks) that can trigger the action. This
value is not included in Backdrop and so it is necessary to add this
as a step in configuration for both simple and advanced actions. In 
this Backdrop version the new config 'triggers' settings are a way of
meeting this need to initialise these settings.

Once actions have been given information about which triggers can be
assigned to them (and one of the options is 'any'), they can be 
assigned to one or more of the triggers by using the Triggers
administrative pages. These are grouped according to their relevance
i.e. separate pages for triggers relating to: Nodes, Comments, Users,
Taxonomy and System.

Actions are that have been assigned triggers are listed on a
page 'Actions - Current Assignments'.



## License

This project is GPL v2 software. See the LICENSE.txt file in this 
directory for complete text.
    
        
## Originator and Current Developer for Backdrop

Graham Oliver (github.com/Graham-72/)



## Acknowledgement

This project makes extensive use of code from both Actions and Triggers
in Drupal 7.


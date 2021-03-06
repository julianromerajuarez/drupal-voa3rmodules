<?php

/**
 * Implements hook_views_data().
 */
function efq_views_views_data() {
  $entity_info = entity_get_info();
  foreach (entity_views_data() as $key => $entity_data) {
    if ($key == 'entity__global') {
      continue;
    }
    if (_efq_views_copy_data($data, $entity_type, $key, $entity_data)) {
      $metadata = entity_get_property_info($entity_type);
      $fields = array();
      // Collect the fields.
      foreach (field_info_instances($entity_type) as $bundle => $field_data) {
        $fields += array_flip(array_keys($field_data));
      }
      $info = $entity_info[$entity_type];
      foreach ($data['efq_' . $entity_type] as $index => &$views_data) {
        unset($views_data['relationship']);
        if (isset($fields[$index]) || isset($views_data['real field'])) {
          _efq_views_get_field_handlers($views_data, $index);
        }
        else {
          _efq_views_get_property_handlers($views_data, $index, $metadata, $info);
        }
      }
      $data['efq_' . $entity_type]['table']['entity type'] = $entity_type;
      $data['efq_' . $entity_type]['table']['base'] = array(
        'title' => 'EntityFieldQuery: ' . $info['label'],
        'help' => t('Uses EntityFieldQuery for querying the !type entity type', array('!type' => $entity_type)),
        'query class' => 'efq_query',
        'field' => $info['entity keys']['id'],
      );
      $data['efq_' . $entity_type] += _efq_views_get_entity_data($entity_type, $info);
    }
  }
  return $data;
}

function _efq_views_copy_data(&$data, &$entity_type, $key, $entity_data) {
  if (substr($key, 0, 7) == 'entity_') {
    $return = TRUE;
    $entity_type = substr($key, 7);
  }
  elseif (substr($key, 0, 13) == 'views_entity_') {
    $return = FALSE;
    $entity_type = substr($key, 13);
  }
  if (!isset($data['efq_' . $entity_type])) {
    $data['efq_' . $entity_type] = array();
  }
  $data['efq_' . $entity_type] += $entity_data;
  return $return;
}

/**
 * Helper for efq_views_views_data()
 * Returns all possible entity metadata variants
 * (entity_id, revision_id, entity_type, bundle).
 *
 * @param $entity_type The selected entity type, if any.
 * @param $info Entity info array.
 */
function _efq_views_get_entity_data($entity_type, $info) {
  $data['entity_id'] = array(
    'group' => t('Entity'),
    'title' => t('Entity ID'),
    'help' => t('The entity ID of the entity.'),
    'field' => array(
      'click sortable' => TRUE,
      'handler' => 'efq_views_handler_field_entity',
    ),
    'filter' => array(
      'handler' => 'efq_views_handler_filter_entity_integer',
    ),
    'sort' => array(
      'handler' => 'efq_views_handler_sort_entity',
    ),
    'argument' => array(
     'handler' => 'efq_views_handler_argument_entity_integer',
    ),
  );
  $data['revision_id'] = array(
    'group' => t('Entity'),
    'title' => t('Revision ID'),
    'help' => t('The revision ID of the entity revision.'),
    'field' => array(
      'click sortable' => TRUE,
      'handler' => 'efq_views_handler_field_entity',
    ),
    'filter' => array(
      'handler' => 'efq_views_handler_filter_entity_integer',
    ),
    'sort' => array(
      'handler' => 'efq_views_handler_sort_entity',
    ),
    'argument' => array(
     'handler' => 'efq_views_handler_argument_entity_integer',
    ),
  );
  if ($entity_type != 'comment' && $entity_type != 'taxonomy_term') {
    $data['bundle'] = array(
      'group' => t('Entity'),
      'title' => t('Bundle'),
      'help' => t('The entity bundle (for example, "article", "page", etc for nodes).'),
      'field' => array(
        'click sortable' => TRUE,
        'handler' => 'efq_views_handler_field_entity',
      ),
      'filter' => array(
        'entity_type' => $entity_type,
        'handler' => 'efq_views_handler_filter_entity_bundle',
      ),
      'sort' => array(
        'handler' => 'efq_views_handler_sort_entity',
      ),
      'argument' => array(
       'handler' => 'efq_views_handler_argument_entity_bundle',
      ),
    );
  }

  $data['bundle_label'] = array(
    'group' => t('Entity'),
    'title' => t('Bundle label'),
    'help' => t('The entity bundle label (for example, "Article", "Basic page", etc for nodes).'),
    'field' => array(
      'click sortable' => FALSE,
      'handler' => 'efq_views_handler_field_entity',
    ),
  );

  $data['label'] = array(
    'group' => t('Entity'),
    'title' => t('Label'),
    'help' => t('The entity label (for example, node title for nodes).'),
    'field' => array(
      'click sortable' => FALSE,
      'handler' => 'efq_views_handler_field_entity_label',
    ),
  );
  // We can only do click sorting and filters/sorts/arguments when the label is
  // stored in the database, instead of being generated in a callback. As
  // labels are not supported by entityCondition/entityOrderBy, treat them as
  // property for filtering and sorting.
  if (isset($info['entity keys']['label']) && !isset($info['label callback'])) {
    $data['label']['field'] = array(
      'click sortable' => TRUE,
      'handler' => 'efq_views_handler_field_entity_label',
      'label column' => $info['entity keys']['label'],
    );
    $data['label']['filter'] = array(
      'handler' => 'efq_views_handler_filter_property_string',
      'field' => $info['entity keys']['label'],
    );
    $data['label']['sort'] = array(
      'handler' => 'efq_views_handler_sort_property',
      'field' => $info['entity keys']['label'],
    );
    $data['label']['argument'] = array(
      'handler' => 'efq_views_handler_argument_property_string',
      'field' => $info['entity keys']['label'],
    );
  }

  return $data;
}

/**
 * Helper for efq_views_views_data()
 * Returns defined fields & their columns for the given entity type
 * (or, if absent, all entity types).
 *
 * Based on field_views_field_default_views_data() from field.views.inc
 */
function _efq_views_get_field_handlers(&$views_data, $field_name) {
  $column = FALSE;
  if (isset($views_data['real field'])) {
    list($field_name, $column) = explode(':', $views_data['real field']);
  }
  $field = field_info_field($field_name);
  // This will be handled later when a real field props up.
  if (count($field['columns']) > 1 && !$column) {
    return;
  }
  foreach ($field['columns'] as $column => $attributes) {
    $allow_sort = TRUE;

    // Identify likely filters and arguments for each column based on field type.
    switch ($attributes['type']) {
      case 'int':
      case 'mediumint':
      case 'tinyint':
      case 'bigint':
      case 'serial':
      case 'numeric':
      case 'float':
        $filter = 'efq_views_handler_filter_field_numeric';
        $argument = 'efq_views_handler_argument_field_numeric';
        break;
      case 'text':
      case 'blob':
        // It does not make sense to sort by blob or text.
        $allow_sort = FALSE;
      default:
        $filter = 'efq_views_handler_filter_field_string';
        $argument = 'efq_views_handler_argument_field_string';
        break;
    }
    switch ($field['module']) {
      case 'list':
        $filter = 'efq_views_handler_filter_field_list';
        break;
      case 'taxonomy':
        $filter = 'efq_views_handler_filter_term_reference';
        $argument = 'efq_views_handler_argument_field_numeric';
        break;
    }
    $views_data['argument'] = array(
      'field' => $column,
      'handler' => $argument,
      'field_name' => $field['field_name'],
      'empty field name' => t('<No value>'),
    );
    $views_data['filter'] = array(
      'field' => $column,
      'handler' => $filter,
      'field_name' => $field_name,
      'allow empty' => TRUE,
    );
    $views_data['field']['click sortable'] = $allow_sort;
    if (!empty($allow_sort)) {
      $views_data['sort'] = array(
        'field' => $column,
        'handler' => 'efq_views_handler_sort_field',
        'field_name' => $field_name,
      );
    }
  }
}

/**
 * Helper for efq_views_views_data()
 * Returns the property data.
 *
 * @param $entity_type Entity type for which the properties are being loaded.
 * @param $info Entity info array for the given entity type.
 */
function _efq_views_get_property_handlers(&$views_data, $property, $metadata, $entity_info) {
  if (!isset($metadata['properties'][$property])) {
    return;
  }
  $property_info = $metadata['properties'][$property];
  // Avoid generating a million notices.
  $property_info += array('type' => '');
  $filter = '';
  $argument = '';

  // Identify likely handlers for each property, based on property type.
  switch ($property_info['type']) {
    case 'integer':
    case 'decimal':
    case 'duration':
      $filter = 'efq_views_handler_filter_property_numeric';
      $argument = 'efq_views_handler_argument_property_numeric';
      break;
    case 'boolean':
      // Views proper doesn't let boolean fields have argument handlers,
      // so we do not support that for properties. (Boolean field API
      // fields are actually stored as an integer so that will work.)
      $filter = 'efq_views_handler_filter_property_boolean';
      break;
    case 'date':
      $filter = 'efq_views_handler_filter_property_date';
      break;
    case 'token':
    case 'text':
    case 'uri':
      if (isset($property_info['options list'])) {
        $filter = 'efq_views_handler_filter_property_in_operator';
        $argument = 'efq_views_handler_argument_property_string';
        break;
      }
      $filter = 'efq_views_handler_filter_property_string';
      $argument = 'efq_views_handler_argument_property_string';
      break;
  }

  if ($filter) {
    $views_data['filter'] = array(
      'handler' => $filter,
      'options callback' => isset($property_info['options list']) ? $property_info['options list'] : NULL,
    );
  }
  if ($argument) {
    $views_data['argument'] = array(
      'handler' => $argument,
    );
  }
  // This property is a db column, we can sort it.
  if (in_array($property, $entity_info['schema_fields_sql']['base table'])) {
    $views_data['field']['click sortable'] = TRUE;
    $views_data['sort']['handler'] = 'efq_views_handler_sort_property';
  }
  else {
    $views_data['field']['click sortable'] = FALSE;
  }
}

/**
 * Implements hook_views_plugins().
 */
function efq_views_views_plugins() {
  $plugins = array(
    'query' => array(
      'efq_query' => array(
        'title' => t('EntityFieldQuery'),
        'help' => t('Uses EntityFieldQuery for querying entities and fields.'),
        'handler' => 'efq_views_plugin_query',
      ),
    ),
  );

  return $plugins;
}

/**
 * Implements hook_views_plugins_alter().
 *
 * This allows us to use the node view style with EntityFieldQuery: Node
 * and comment view style with EntityFieldQuery: Comment
 * (just like with regular nodes and comments).
 *
 * Note that these style plugins do an entity_load() even though they already
 * get fully loaded entities from our query engine.
 * However, entity_load has internal caching, so the only real overhead is an
 * extra function call.
 */
function efq_views_views_plugins_alter(&$plugins) {
  $plugins['row']['node']['base'][] = "efq_node";
  $plugins['row']['comment']['base'][] = "efq_comment";
}

/**
 * Cast values to int if necessary and create delta.
 *
 * @param $handler
 *   The Views handler object.
 * @param $value
 *   The value for the operation. Can be a scalar or an array.
 * @param $delta
 *   The input value is disregarded, it's only used for returning delta.
 *
 * @return
 *   Same as $value, casted to int if necessary.
 */
function efq_views_extract_delta($handler) {
  return isset($handler->options['delta']) && is_numeric($handler->options['delta']) ? $handler->options['delta'] : NULL;
}

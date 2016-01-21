<?php
/**
 * @file
 * Plugin implementation for the exclusion context condition.
 */

/**
 * Implementation of context condition for excluding node types by path.
 *
 * Extends the node condition and brings in the functionality from the path
 * condition. The goal is to allow the ability to exclude based on paths.
 *
 * @category Module
 *
 * @package context_condition_exclusion
 *
 * @see context_condition_node, context_condition_path
 */
// @codingStandardsIgnoreLine
class context_exclusion_context_condition_exclusion extends context_condition_node {

  const MATCH_NOT_FOUND = 0;
  const MATCH_FOUND = 1;
  const MATCH_EXCLUDE = 2;

  /**
   * Implments the condition options form.
   *
   * @see context_condition_node::options_form()
   */
  // @codingStandardsIgnoreLine
  public function options_form($context) {

    $defaults = $this->fetch_from_context($context, 'options');

    $form = parent::options_form($context);

    $form['paths'] = array(
      '#title' => t('Paths'),
      '#type' => 'textarea',
      '#description' => t('Set this context when the node types combined with the paths match. Put each path on a separate line. You can use the <code>*</code> character (asterisk) as a wildcard and the <code>~</code> character (tilde) to exclude one or more paths. Use &lt;front&gt; for the site front page.'),
      '#default_value' => isset($defaults['paths']) ? implode("\n", $defaults['paths']) : '',
    );

    $form['reverse'] = array(
      '#title' => t('Reverse the result of the check'),
      '#type' => 'checkbox',
      '#description' => t('If checked then the result of the node type and exclusion checks will be reversed. This lets you stop a context from working on certain nodes.'),
      '#default_value' => isset($defaults['reverse']),
      '#access' => FALSE,
    );

    return $form;
  }

  /**
   * Handle submission of the options form.
   *
   * @see context_condition_node::options_form_submit()
   */
  // @codingStandardsIgnoreLine
  public function options_form_submit($values) {

    $values = parent::options_form_submit($values);

    if (!empty($values['paths'])) {

      $values['paths'] = $this->contextConditionPathOptionsFormSubmit($values);
    }

    return $values;
  }

  /**
   * Execute.
   *
   * Borrows logic heavily from context_condition_node but expands to include
   * path logic.
   *
   * @see context_condition_node::execute()
   * @see context_condition_path::execute()
   */
  // @codingStandardsIgnoreLine
  public function execute($node, $operation) {

    if (!$this->condition_used()) {

      return NULL;
    }

    $current_path = array(drupal_get_path_alias($_GET['q']));
    if ($current_path[0] != $_GET['q']) {

      $current_path[] = $_GET['q'];
    }

    foreach ($this->get_contexts() as $context) {

      $options = $this->fetch_from_context($context, 'options');

      $matched_on_node = $this->matchOnNodeType($node->type, $operation, $context, $options);
      $match_result = $this->matchOnPath($current_path, $context, $options);

      $matched_on_path = self::MATCH_EXCLUDE !== $match_result;
      $excluded_path = self::MATCH_EXCLUDE === $match_result;

      $condition_met = !$excluded_path && ($matched_on_node && $matched_on_path);

      if (!empty($options['reverse'])) {

        $condition_met = !$condition_met;
      }

      if ($condition_met) {

        $this->condition_met($context, $node->type);
      }
    }
  }

  /**
   * Check if the given path matches the configured paths.
   *
   * @param string $path
   *   The path to check.
   * @param object $context
   *   The context object.
   * @param array $options
   *   The configured options. Must have a paths array set.
   *
   * @return int
   *   One of self::MATCH_FOUND, self::MATCH_NOT_FOUND, self::MATCH_EXCLUDE.
   */
  protected function matchOnPath($path, $context, $options) {

    return $this->match($path, $options['paths'], TRUE);
  }

  /**
   * Check if the given node matches the configured options.
   *
   * @param object $node_type
   *   The node type to check.
   * @param string $operation
   *   The page operation.
   * @param object $context
   *   The context object.
   * @param array $options
   *   The configured options. Must have a paths array set.
   *
   * @return bool
   *   TRUE if the node type matches, FALSE otherwise.
   */
  protected function matchOnNodeType($node_type, $operation, $context, $options) {

    // The "match on node forms" option was enabled.
    $match_on_form = isset($options['node_form']) ?
      in_array($options['node_form'], array(CONTEXT_NODE_FORM, CONTEXT_NODE_FORM_ONLY)) :
      FALSE;

    // If configured to show on forms and we're on a form.
    $match_on_form = 'form' === $operation && $match_on_form;

    // If set to only show on forms and we're not on a form.
    $match_on_not_form = 'form' !== $operation && CONTEXT_NODE_FORM_ONLY !== $options['node_form'];

    $operation_match = $match_on_form || $match_on_not_form;

    $selected_nodes = $this->fetch_from_context($context, 'values');
    $selected_nodes = array_filter($selected_nodes);
    if ($operation_match && in_array($node_type, $selected_nodes)) {

      return TRUE;
    }

    return FALSE;
  }

  /**
   * Mimic condition_form_submit() from context_condition_path.
   *
   * @param array $values
   *   The values array witha paths key.
   *
   * @return array
   *   An array of paths.
   *
   * @see context_condition_path::condition_form_submit()
   */
  protected function contextConditionPathOptionsFormSubmit($values) {

    $parsed = array();
    $items = explode("\n", $values['paths']);
    if (!empty($items)) {
      foreach ($items as $v) {
        $v = trim($v);
        if (!empty($v)) {
          $parsed[$v] = $v;
        }
      }
    }

    return $parsed;
  }

  /**
   * Match the subject against a set of regex patterns.
   *
   * Similar to drupal_match_path() but also handles negation through the use
   * of the ~ character.
   *
   * This is a modified version of the logic found in
   * context_condition_path::match() in order to tell us if we matched, hit an
   * exclusion, or couldn't match.
   *
   * @param mixed $subject
   *   The subject string or an array of strings to be matched.
   * @param array $patterns
   *   An array of patterns. Any patterns that begin with ~ are considered
   *   negative or excluded conditions.
   * @param bool $path
   *   Whether the given subject should be matched as a Drupal path. If TRUE,
   *   '<front>' will be replaced with the site frontpage when matching against
   *   $patterns.
   *
   * @see context_condition_path::match()
   */
  protected function match($subject, $patterns, $path = FALSE) {

    static $regexps;
    $match = self::MATCH_NOT_FOUND;
    $positives = $negatives = 0;
    $subject = !is_array($subject) ? array($subject) : $subject;
    foreach ($patterns as $pattern) {
      if (empty($pattern)) {
        continue;
      }

      if (strpos($pattern, '~') === 0) {
        $negate = TRUE;
        $negatives++;
      }
      else {
        $negate = FALSE;
        $positives++;
      }
      $pattern = ltrim($pattern, '~');
      if (!isset($regexps[$pattern])) {
        if ($path) {
          $regexps[$pattern] = '/^(' . preg_replace(
            array(
              '/(\r\n?|\n)/',
              '/\\\\\*/',
              '/(^|\|)\\\\<front\\\\>($|\|)/',
            ),
            array(
              '|',
              '.*',
              '\1' . preg_quote(variable_get('site_frontpage', 'node'), '/') . '\2',
            ),
            preg_quote($pattern, '/')) . ')$/';
        }
        else {
          $regexps[$pattern] = '/^(' . preg_replace(array('/(\r\n?|\n)/', '/\\\\\*/'), array('|', '.*'), preg_quote($pattern, '/')) . ')$/';
        }
      }
      foreach ($subject as $value) {
        if (preg_match($regexps[$pattern], $value)) {
          if ($negate) {
            return self::MATCH_EXCLUDE;
          }
          $match = self::MATCH_FOUND;
        }
      }
    }

    return $match;
  }

}

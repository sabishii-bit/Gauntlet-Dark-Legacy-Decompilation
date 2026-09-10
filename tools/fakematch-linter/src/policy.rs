//! Review policy: reasoned exceptions never delete rows, they mark them.

use std::collections::HashMap;

use crate::config::Policy;
use crate::finding::Finding;

/// Apply inline suppressions, fingerprint exceptions and pragma approvals.
///
/// * Inline comments waive any source rule they name.
/// * `#pragma` warnings stay visible; legacy policy entries cannot hide them.
/// * FM008 (postprocessor inventory) and direct (non-macro) FM005 assembly are
///   never waived by fingerprint.
pub fn apply(findings: &mut [Finding], policy: &Policy) {
    let exceptions: HashMap<&str, &str> = policy
        .exceptions
        .iter()
        .map(|e| (e.fingerprint.as_str(), e.reason.as_str()))
        .collect();
    let mut used = vec![0u64; policy.pragma_allowlist.len()];
    for row in findings.iter_mut() {
        row.suppressed = false;
        row.review_reason = None;
        row.suppression_source = None;
        let pragma = row.is_pragma();
        if let Some(inline) = &row.inline_suppression {
            row.suppressed = true;
            row.review_reason = Some(inline.reason.clone());
            row.suppression_source = Some("source-comment".to_string());
            continue;
        }
        if pragma {
            continue;
        }
        let mut reason = exceptions.get(row.fingerprint.as_str()).map(|r| r.to_string());
        if row.rule == "FM008" {
            reason = None;
        }
        if row.rule == "FM005" && row.detail_str("macro").is_none() {
            reason = None;
        }
        if reason.is_none() && row.rule == "FM006" {
            let scope = row.detail_str("pragma_scope").unwrap_or("");
            let directive = row.detail_str("directive").unwrap_or("");
            for (i, e) in policy.pragma_allowlist.iter().enumerate() {
                if e.path == row.path && e.scope == scope && e.directive == directive && used[i] < e.count {
                    used[i] += 1;
                    reason = Some(e.reason.clone());
                    break;
                }
            }
        }
        if let Some(reason) = reason {
            row.suppressed = true;
            row.review_reason = Some(reason);
        }
    }
}

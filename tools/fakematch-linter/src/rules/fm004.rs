//! FM004: float/address-shaped numeric byte array.
//!
//! `static const u8 table[] = {0x3f,0x80,0,0,...}` whose bytes decode as
//! plausible big-endian floats or GameCube addresses probably has a real
//! type. This is a shape heuristic only.

use regex::Regex;

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm004Config, Severity};
use crate::finding::Confidence;
use crate::parse::{self, has_descendant, node_text};

use super::Rule;

pub struct Fm004 {
    severity: Severity,
    head: Regex,
    min_bytes: usize,
    address_range: [u64; 2],
    min_address_words: usize,
    float_ratio: f64,
    float_magnitude: [f64; 2],
}

impl Fm004 {
    pub fn new(config: &Fm004Config) -> Self {
        let types = config
            .byte_types
            .iter()
            .map(|t| {
                t.split_whitespace()
                    .map(regex::escape)
                    .collect::<Vec<_>>()
                    .join(r"\s+")
            })
            .collect::<Vec<_>>()
            .join("|");
        let head = Regex::new(&format!(r"^static\s+const\s+(?:{types})\s+(\w+)\s*\["))
            .expect("escaped type names");
        Fm004 {
            severity: config.severity,
            head,
            min_bytes: config.min_bytes,
            address_range: config.address_range,
            min_address_words: config.min_address_words,
            float_ratio: config.float_ratio,
            float_magnitude: config.float_magnitude,
        }
    }

    fn shapes(&self, data: &[u8]) -> Vec<String> {
        let mut shapes = Vec::new();
        if data.len().is_multiple_of(4) {
            let words: Vec<u64> = data
                .chunks(4)
                .map(|c| u32::from_be_bytes([c[0], c[1], c[2], c[3]]) as u64)
                .collect();
            if words.len() >= self.min_address_words
                && words.iter().all(|&w| {
                    self.address_range[0] <= w && w < self.address_range[1] && w % 4 == 0
                })
            {
                shapes.push("GameCube-address-shaped words".to_string());
            }
        }
        for width in [4usize, 8] {
            if !data.len().is_multiple_of(width) || data.len() / width < 2 {
                continue;
            }
            let values: Vec<f64> = data
                .chunks(width)
                .map(|c| {
                    if width == 4 {
                        f32::from_be_bytes([c[0], c[1], c[2], c[3]]) as f64
                    } else {
                        f64::from_be_bytes([c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]])
                    }
                })
                .collect();
            let plausible = values
                .iter()
                .filter(|v| {
                    v.is_finite()
                        && self.float_magnitude[0] <= v.abs()
                        && v.abs() <= self.float_magnitude[1]
                })
                .count();
            if plausible as f64 / values.len() as f64 >= self.float_ratio {
                shapes.push(format!("big-endian float{}-shaped values", width * 8));
            }
        }
        shapes
    }
}

fn parse_item(s: &str) -> Option<u64> {
    let s = s.trim().trim_end_matches(['u', 'U']);
    if let Some(hex) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        u64::from_str_radix(hex, 16).ok()
    } else if !s.is_empty() && s.bytes().all(|b| b.is_ascii_digit()) {
        s.parse().ok()
    } else {
        None
    }
}

impl Rule for Fm004 {
    fn id(&self) -> &'static str {
        "FM004"
    }

    fn title(&self) -> &'static str {
        "Float/address-shaped numeric byte array"
    }

    fn check(&self, a: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        for &n in &a.source {
            if n.kind() != "declaration" || !has_descendant(n, "initializer_list") {
                continue;
            }
            let (x, y) = (n.start_byte(), n.end_byte());
            let clean = String::from_utf8_lossy(&a.lex.clean[x..y]);
            let Some(cap) = self.head.captures(&clean) else { continue };
            let variable = cap[1].to_string();
            // Take the first (outermost, flat) initializer list.
            let Some(open) = clean.find('{') else { continue };
            let Some(close) = clean[open..].find('}') else { continue };
            let body = &clean[open + 1..open + close];
            if body.contains('{') {
                continue;
            }
            let items: Vec<&str> = body.split(',').map(str::trim).filter(|s| !s.is_empty()).collect();
            if items.is_empty() {
                continue;
            }
            let mut data = Vec::with_capacity(items.len());
            let mut ok = true;
            for item in items {
                match parse_item(item) {
                    Some(v) if v <= 255 => data.push(v as u8),
                    _ => {
                        ok = false;
                        break;
                    }
                }
            }
            if !ok || data.len() < self.min_bytes {
                continue;
            }
            let shapes = self.shapes(&data);
            if shapes.is_empty() {
                continue;
            }
            let _ = (parse::node_text as fn(_, _) -> _, node_text as fn(_, _) -> _);
            sink.emit(
                "FM004",
                x,
                y,
                &format!("{}; inspect consumers before retyping.", shapes.join("; ")),
                Confidence::Heuristic,
                self.severity,
                details([
                    ("variable", serde_json::json!(variable)),
                    ("byte_count", serde_json::json!(data.len())),
                ]),
            );
        }
    }
}

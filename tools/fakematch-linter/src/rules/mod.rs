//! Source rules. Each rule is an independent module that reads the shared
//! per-file [`Analysis`] and emits findings into a [`Sink`].

use crate::analysis::{Analysis, Sink};
use crate::config::Config;

pub mod fm001;
pub mod fm002;
pub mod fm003;
pub mod fm004;
pub mod fm005;
pub mod fm006;
pub mod fm007;
pub mod fm009;

pub trait Rule: Send + Sync {
    /// Rule identifier, e.g. `FM001`.
    fn id(&self) -> &'static str;
    /// One-line title.
    fn title(&self) -> &'static str;
    /// Inspect one file and report candidates.
    fn check(&self, analysis: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>);
}

/// Build the enabled source rules from configuration.
pub fn build(config: &Config) -> Vec<Box<dyn Rule>> {
    let r = &config.rules;
    let mut rules: Vec<Box<dyn Rule>> = Vec::new();
    if r.fm001.enabled {
        rules.push(Box::new(fm001::Fm001::new(&r.fm001)));
    }
    if r.fm002.enabled {
        rules.push(Box::new(fm002::Fm002::new(&r.fm002)));
    }
    if r.fm003.enabled {
        rules.push(Box::new(fm003::Fm003::new(&r.fm003)));
    }
    if r.fm004.enabled {
        rules.push(Box::new(fm004::Fm004::new(&r.fm004)));
    }
    if r.fm005.enabled {
        rules.push(Box::new(fm005::Fm005::new(&r.fm005)));
    }
    if r.fm006.enabled {
        rules.push(Box::new(fm006::Fm006::new(&r.fm006)));
    }
    if r.fm007.enabled {
        rules.push(Box::new(fm007::Fm007::new(&r.fm007)));
    }
    if r.fm009.enabled {
        rules.push(Box::new(fm009::Fm009::new(&r.fm009)));
    }
    rules
}

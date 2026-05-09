use std::collections::HashMap;
use std::path::Path;
use toml::Value;
use log::info;

#[derive(Debug)]
pub struct Config {
    pub hotkeys: HashMap<String, String>,
}

impl Config {
    pub fn load(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let content = std::fs::read_to_string(path)?;
        let value: Value = content.parse()?;

        let mut hotkeys = HashMap::new();
        if let Some(h) = value.get("hotkeys").and_then(|v| v.as_table()) {
            for (key, val) in h {
                if let Some(v) = val.as_str() {
                    hotkeys.insert(key.clone(), v.to_lowercase());
                }
            }
        }

        info!("Loaded {} hotkeys", hotkeys.len());

        Ok(Self { hotkeys })
    }
}
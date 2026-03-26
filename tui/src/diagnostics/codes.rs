/// Diagnostic categories matching engine subsystems.
/// Code format: CATEGORY-S### (e.g., MANIFEST-E001, HOOK-W003)

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Category {
    Boot,
    Manifest,
    Hook,
    Area,
    Warp,
    Resource,
    Npc,
    Quest,
    Trigger,
    Story,
    Save,
    Ipc,
    Pack,
}

impl Category {
    /// Prefix string used in diagnostic codes.
    pub fn prefix(&self) -> &'static str {
        match self {
            Category::Boot => "BOOT",
            Category::Manifest => "MANIFEST",
            Category::Hook => "HOOK",
            Category::Area => "AREA",
            Category::Warp => "WARP",
            Category::Resource => "RESOURCE",
            Category::Npc => "NPC",
            Category::Quest => "QUEST",
            Category::Trigger => "TRIGGER",
            Category::Story => "STORY",
            Category::Save => "SAVE",
            Category::Ipc => "IPC",
            Category::Pack => "PACK",
        }
    }

    /// Parse a category from its prefix string.
    #[allow(dead_code)]
    pub fn from_prefix(s: &str) -> Option<Self> {
        match s {
            "BOOT" => Some(Category::Boot),
            "MANIFEST" => Some(Category::Manifest),
            "HOOK" => Some(Category::Hook),
            "AREA" => Some(Category::Area),
            "WARP" => Some(Category::Warp),
            "RESOURCE" => Some(Category::Resource),
            "NPC" => Some(Category::Npc),
            "QUEST" => Some(Category::Quest),
            "TRIGGER" => Some(Category::Trigger),
            "STORY" => Some(Category::Story),
            "SAVE" => Some(Category::Save),
            "IPC" => Some(Category::Ipc),
            "PACK" => Some(Category::Pack),
            _ => None,
        }
    }
}

/// A parsed diagnostic code (e.g., MANIFEST-E001).
#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct DiagnosticCode {
    pub category: Category,
    pub severity: super::severity::Severity,
    pub number: u16,
}

impl DiagnosticCode {
    /// Parse a code string like "MANIFEST-E001".
    #[allow(dead_code)]
    pub fn parse(code: &str) -> Option<Self> {
        let (prefix, rest) = code.rsplit_once('-')?;
        let category = Category::from_prefix(prefix)?;

        let mut chars = rest.chars();
        let severity_char = chars.next()?;
        let severity = match severity_char {
            'E' => super::severity::Severity::Error,
            'W' => super::severity::Severity::Warning,
            'H' => super::severity::Severity::Hazard,
            _ => return None,
        };

        let number: u16 = chars.as_str().parse().ok()?;

        Some(Self {
            category,
            severity,
            number,
        })
    }

    /// Format as string (e.g., "MANIFEST-E001").
    #[allow(dead_code)]
    pub fn to_string(&self) -> String {
        format!(
            "{}-{}{}",
            self.category.prefix(),
            self.severity.code_letter(),
            format!("{:03}", self.number)
        )
    }
}

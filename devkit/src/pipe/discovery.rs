use windows::{
    core::PCWSTR,
    Win32::Storage::FileSystem::{FindClose, FindFirstFileW, FindNextFileW, WIN32_FIND_DATAW},
};

/// Enumerate all named pipes matching `\\.\pipe\efl-*`.
/// Returns a list of full pipe paths (e.g. `\\.\pipe\efl-1234`).
pub fn discover_efl_pipes() -> Vec<String> {
    let pattern = r"\\.\pipe\efl-*";
    let pattern_wide: Vec<u16> = pattern.encode_utf16().chain(std::iter::once(0)).collect();

    let mut results = Vec::new();

    unsafe {
        let mut find_data = WIN32_FIND_DATAW::default();

        // FindFirstFileW returns Result<HANDLE> in windows 0.58
        let handle = match FindFirstFileW(PCWSTR(pattern_wide.as_ptr()), &mut find_data) {
            Ok(h) => h,
            Err(_) => return results, // no matching pipes
        };

        loop {
            let name = wide_to_string(&find_data.cFileName);
            if !name.is_empty() {
                results.push(format!(r"\\.\pipe\{name}"));
            }

            // FindNextFileW returns Result<()>; break on any error (ERROR_NO_MORE_FILES)
            if FindNextFileW(handle, &mut find_data).is_err() {
                break;
            }
        }

        let _ = FindClose(handle);
    }

    results
}

/// Connect to the most recent EFL engine pipe (highest PID = most recently spawned).
/// Returns the full pipe path, or None if no engine is running.
pub fn connect_to_latest_efl_pipe() -> Option<String> {
    let mut pipes = discover_efl_pipes();

    // Sort by PID descending — parse the number after "\\.\pipe\efl-"
    pipes.sort_by(|a, b| {
        let pid_a = parse_pid(a);
        let pid_b = parse_pid(b);
        pid_b.cmp(&pid_a)
    });

    pipes.into_iter().next()
}

fn parse_pid(pipe_path: &str) -> u32 {
    pipe_path
        .rsplit('-')
        .next()
        .and_then(|s| s.parse().ok())
        .unwrap_or(0)
}

fn wide_to_string(wide: &[u16]) -> String {
    let end = wide.iter().position(|&c| c == 0).unwrap_or(wide.len());
    String::from_utf16_lossy(&wide[..end])
}

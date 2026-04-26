// Outbound command pipe writer.
//
// Mirrors the engine-side `CommandPipeListener` (see
// engine/src/ipc/command_pipe.cpp). For every running EFL engine, the engine
// creates a sibling pipe at `\\.\pipe\efl-<pid>-cmd` that accepts JSON Lines.
// The DevKit opens that pipe as a regular file and writes commands like:
//
//   {"type":"reload","payload":{"reason":"devkit-repack"}}
//
// Connections are short-lived: open, write, close. The engine resets the pipe
// after each disconnect so the next command opens a fresh stream.

use std::{
    fs::OpenOptions,
    io::Write,
    path::Path,
    time::{Duration, Instant},
};

#[derive(Debug, Clone, Copy)]
pub struct ReloadCommand<'a> {
    pub reason: &'a str,
}

/// Derive the command pipe name from the event pipe name.
///
/// The event pipe is `\\.\pipe\efl-<pid>` (see `pipe::discovery`).  The
/// command pipe shares the PID with a `-cmd` suffix.
pub fn command_pipe_for_event_pipe(event_pipe_name: &str) -> Option<String> {
    let path = Path::new(event_pipe_name);
    let leaf = path.file_name()?.to_str()?;
    if !leaf.starts_with("efl-") {
        return None;
    }
    Some(format!(r"\\.\pipe\{leaf}-cmd"))
}

/// Send a reload command to the named command pipe.
///
/// Connection retries for up to `timeout_ms` to absorb transient pipe state
/// changes (e.g. the engine just bound the pipe but hasn't yet finished
/// `ConnectNamedPipe`).
pub fn send_reload(command_pipe_name: &str, command: ReloadCommand) -> Result<(), String> {
    let envelope = serde_json::json!({
        "type": "reload",
        "payload": {
            "reason": command.reason,
        }
    });

    write_line(command_pipe_name, &envelope.to_string(), Duration::from_millis(500))
}

/// Send an arbitrary JSON envelope (used by `ping` / future commands).
pub fn send_envelope(
    command_pipe_name: &str,
    envelope: &serde_json::Value,
    timeout: Duration,
) -> Result<(), String> {
    write_line(command_pipe_name, &envelope.to_string(), timeout)
}

/// Ask the engine to re-broadcast its `capabilities.snapshot` event. Useful
/// when the DevKit attaches mid-session and missed the boot-time emission;
/// the engine answers on the outbound pipe with a fresh snapshot envelope.
pub fn request_capabilities(command_pipe_name: &str) -> Result<(), String> {
    let envelope = serde_json::json!({"type": "caps", "payload": {}});
    write_line(
        command_pipe_name,
        &envelope.to_string(),
        Duration::from_millis(500),
    )
}

fn write_line(
    command_pipe_name: &str,
    line: &str,
    timeout: Duration,
) -> Result<(), String> {
    let deadline = Instant::now() + timeout;
    let mut last_err: Option<String> = None;
    while Instant::now() < deadline {
        match OpenOptions::new().write(true).open(command_pipe_name) {
            Ok(mut f) => {
                let mut payload = String::with_capacity(line.len() + 1);
                payload.push_str(line);
                payload.push('\n');
                if let Err(e) = f.write_all(payload.as_bytes()) {
                    return Err(format!("write to {command_pipe_name}: {e}"));
                }
                let _ = f.flush();
                return Ok(());
            }
            Err(e) => {
                last_err = Some(e.to_string());
                std::thread::sleep(Duration::from_millis(40));
            }
        }
    }
    Err(last_err.unwrap_or_else(|| format!("could not open {command_pipe_name}")))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn command_pipe_for_event_pipe_basic() {
        let cmd = command_pipe_for_event_pipe(r"\\.\pipe\efl-12345").unwrap();
        assert_eq!(cmd, r"\\.\pipe\efl-12345-cmd");
    }

    #[test]
    fn command_pipe_for_non_efl_returns_none() {
        assert!(command_pipe_for_event_pipe(r"\\.\pipe\unrelated-1").is_none());
    }
}

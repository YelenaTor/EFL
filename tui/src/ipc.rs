use serde::Deserialize;

/// A message received from the engine DLL via named pipe.
#[derive(Debug, Deserialize)]
pub struct IpcMessage {
    #[serde(rename = "type")]
    pub msg_type: String,
    pub timestamp: String,
    pub payload: serde_json::Value,
}

/// Reads JSON Lines from a Windows named pipe in a dedicated thread.
pub struct PipeReader {
    // TODO: Handle to background reader thread
    // TODO: Channel receiver for parsed IpcMessages
    pipe_name: String,
}

impl PipeReader {
    pub fn new(pipe_name: &str) -> Self {
        Self {
            pipe_name: pipe_name.to_string(),
        }
    }

    /// Start reading from the named pipe in a background thread.
    pub fn start(&mut self) {
        // TODO: Open \\.\pipe\efl-{pid} via windows-rs
        // TODO: Spawn thread that reads lines and parses JSON
        // TODO: Send parsed IpcMessages through a channel
    }

    /// Try to receive the next message (non-blocking).
    pub fn try_recv(&self) -> Option<IpcMessage> {
        // TODO: Check channel for next message
        None
    }
}

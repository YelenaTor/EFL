use std::io::{BufRead, BufReader};
use std::sync::mpsc;
use std::thread;

use serde::Deserialize;

/// A message received from the engine DLL via named pipe.
#[derive(Debug, Clone, Deserialize)]
pub struct IpcMessage {
    #[serde(rename = "type")]
    pub msg_type: String,
    pub timestamp: String,
    pub payload: serde_json::Value,
}

/// Reads JSON Lines from a Windows named pipe in a dedicated thread.
pub struct PipeReader {
    rx: mpsc::Receiver<IpcMessage>,
}

impl PipeReader {
    /// Connect to the given named pipe and start reading JSON Lines in a background thread.
    pub fn connect(pipe_name: &str) -> Self {
        let (tx, rx) = mpsc::channel();
        let pipe_name = pipe_name.to_string();

        thread::spawn(move || {
            // Retry connecting — engine may not have created the pipe yet
            let file = loop {
                match std::fs::File::open(&pipe_name) {
                    Ok(f) => break f,
                    Err(_) => thread::sleep(std::time::Duration::from_millis(500)),
                }
            };

            let reader = BufReader::new(file);
            for line in reader.lines() {
                let Ok(line) = line else { break };
                if line.trim().is_empty() {
                    continue;
                }
                if let Ok(msg) = serde_json::from_str::<IpcMessage>(&line) {
                    if tx.send(msg).is_err() {
                        break; // receiver dropped
                    }
                }
            }
        });

        Self { rx }
    }

    /// Create a PipeReader from a pre-built channel (used by demo mode).
    pub fn from_channel(rx: mpsc::Receiver<IpcMessage>) -> Self {
        Self { rx }
    }

    /// Try to receive the next message (non-blocking).
    pub fn try_recv(&self) -> Option<IpcMessage> {
        self.rx.try_recv().ok()
    }
}

use futures_util::StreamExt;
use base64::{engine::general_purpose::STANDARD as BASE64, Engine as _};
use mdns_sd::{ServiceDaemon, ServiceEvent};
use reqwest::{multipart, Client};
use serde::{Deserialize, Serialize};
use std::{
    fs,
    net::IpAddr,
    path::{Path, PathBuf},
    time::{Duration, SystemTime, UNIX_EPOCH},
};
use tauri::{Manager, State};
use tokio::{fs::File, io::AsyncWriteExt, sync::Mutex};
use tokio_util::io::ReaderStream;

#[derive(Debug, Clone, Serialize, Deserialize)]
struct DeviceTarget {
    base_url: String,
}

#[derive(Debug)]
struct AppState {
    client: Client,
    target: Mutex<Option<DeviceTarget>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct DeviceStatus {
    ok: bool,
    device: Option<String>,
    hostname: Option<String>,
    version: Option<String>,
    ap_ssid: Option<String>,
    ap_ip: Option<String>,
    sta_connected: Option<bool>,
    sta_ssid: Option<String>,
    sta_ip: Option<String>,
    sd: Option<bool>,
    battery_percent: Option<u8>,
    selected_id: Option<i32>,
    item_count: Option<usize>,
    selected_type: Option<String>,
    wifi: Option<String>,
    #[serde(default)]
    base_url: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct ClipItem {
    id: usize,
    #[serde(rename = "type")]
    item_type: String,
    title: String,
    preview: Option<String>,
    size: Option<u64>,
    selected: Option<bool>,
    mime: Option<String>,
    text: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct ItemsResponse {
    items: Vec<ClipItem>,
}

#[derive(Debug, Clone, Deserialize)]
struct ApiOk {
    ok: bool,
    message: Option<String>,
    error: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
struct DownloadResult {
    path: String,
    filename: String,
}

#[derive(Debug, Clone, Serialize)]
struct PreviewResult {
    data_url: String,
}

#[derive(Debug, Clone, Serialize)]
struct PdfPreviewResult {
    path: String,
    page_count: usize,
}

#[derive(Debug, Clone, Serialize)]
struct ClipboardFilesResult {
    paths: Vec<String>,
}

fn normalize_base_url(input: &str) -> String {
    let trimmed = input.trim().trim_end_matches('/');
    if trimmed.starts_with("http://") || trimmed.starts_with("https://") {
        trimmed.to_string()
    } else {
        format!("http://{trimmed}")
    }
}

fn app_config_path(app: &tauri::AppHandle) -> PathBuf {
    app.path()
        .app_config_dir()
        .unwrap_or_else(|_| {
            dirs::config_dir()
                .unwrap_or_else(std::env::temp_dir)
                .join("ClipBridge")
        })
        .join("device.json")
}

fn read_last_target(app: &tauri::AppHandle) -> Option<DeviceTarget> {
    let path = app_config_path(app);
    let data = fs::read_to_string(path).ok()?;
    serde_json::from_str::<DeviceTarget>(&data).ok()
}

fn write_last_target(app: &tauri::AppHandle, target: &DeviceTarget) {
    let path = app_config_path(app);
    if let Some(parent) = path.parent() {
        let _ = fs::create_dir_all(parent);
    }
    if let Ok(data) = serde_json::to_string_pretty(target) {
        let _ = fs::write(path, data);
    }
}

async fn probe_status(client: &Client, base_url: &str) -> Result<DeviceStatus, String> {
    let url = format!("{}/api/status", base_url.trim_end_matches('/'));
    let response = client
        .get(url)
        .timeout(Duration::from_millis(1400))
        .send()
        .await
        .map_err(|error| error.to_string())?;

    if !response.status().is_success() {
        return Err(format!("HTTP {}", response.status()));
    }

    let mut status = response
        .json::<DeviceStatus>()
        .await
        .map_err(|error| error.to_string())?;

    let device_ok = status
        .device
        .as_deref()
        .map(|device| device.eq_ignore_ascii_case("ClipBridge"))
        .unwrap_or_else(|| status.selected_type.is_some());

    if !device_ok {
        return Err("not a ClipBridge device".into());
    }

    status.base_url = Some(base_url.to_string());
    Ok(status)
}

async fn activate_target(
    app: &tauri::AppHandle,
    state: &AppState,
    base_url: String,
) -> Result<DeviceStatus, String> {
    let status = probe_status(&state.client, &base_url).await?;
    let target = DeviceTarget { base_url };
    *state.target.lock().await = Some(target.clone());
    write_last_target(app, &target);
    Ok(status)
}

fn mdns_candidates() -> Vec<String> {
    let mut candidates = Vec::new();
    let Ok(mdns) = ServiceDaemon::new() else {
        return candidates;
    };
    let Ok(receiver) = mdns.browse("_http._tcp.local.") else {
        let _ = mdns.shutdown();
        return candidates;
    };

    let deadline = std::time::Instant::now() + Duration::from_millis(1200);
    while std::time::Instant::now() < deadline {
        let remaining = deadline.saturating_duration_since(std::time::Instant::now());
        match receiver.recv_timeout(remaining.min(Duration::from_millis(250))) {
            Ok(ServiceEvent::ServiceResolved(info)) => {
                let name = info.fullname.to_ascii_lowercase();
                let host = info.host.to_ascii_lowercase();
                if name.contains("clipbridge") || host.starts_with("clipbridge") {
                    for addr in info.addresses.iter() {
                        let ip: IpAddr = addr.to_ip_addr();
                        if ip.is_ipv4() {
                            candidates.push(format!("http://{}:{}", ip, info.port));
                        }
                    }
                }
            }
            Ok(_) => {}
            Err(_) => {}
        }
    }
    let _ = mdns.stop_browse("_http._tcp.local.");
    let _ = mdns.shutdown();
    candidates
}

async fn current_base_url(app: &tauri::AppHandle, state: &AppState) -> Result<String, String> {
    if let Some(target) = state.target.lock().await.clone() {
        return Ok(target.base_url);
    }

    if let Some(target) = read_last_target(app) {
        *state.target.lock().await = Some(target.clone());
        return Ok(target.base_url);
    }

    Err("ClipBridge not found".into())
}

async fn fetch_item(client: &Client, base_url: &str, id: usize) -> Result<ClipItem, String> {
    let response = client
        .get(format!("{base_url}/api/items/{id}"))
        .timeout(Duration::from_millis(1800))
        .send()
        .await
        .map_err(|error| error.to_string())?;

    if !response.status().is_success() {
        return Err(format!("HTTP {}", response.status()));
    }

    response.json::<ClipItem>().await.map_err(|error| error.to_string())
}

#[tauri::command]
async fn discover_clipbridge(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    manual_address: Option<String>,
) -> Result<DeviceStatus, String> {
    let mut candidates = Vec::<String>::new();

    if let Some(address) = manual_address.filter(|value| !value.trim().is_empty()) {
        candidates.push(normalize_base_url(&address));
    }

    candidates.push("http://clipbridge.local".into());

    if let Some(target) = read_last_target(&app) {
        candidates.push(target.base_url);
    }

    candidates.extend(mdns_candidates());
    candidates.push("http://192.168.4.1".into());

    candidates.dedup();
    let mut last_error = "ClipBridge not found".to_string();

    for base_url in candidates {
        match activate_target(&app, &state, base_url.clone()).await {
            Ok(status) => return Ok(status),
            Err(error) => last_error = format!("{base_url}: {error}"),
        }
    }

    *state.target.lock().await = None;
    Err(last_error)
}

#[tauri::command]
async fn get_status(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
) -> Result<DeviceStatus, String> {
    let base_url = match current_base_url(&app, &state).await {
        Ok(base_url) => base_url,
        Err(_) => return discover_clipbridge(app, state, None).await,
    };
    match probe_status(&state.client, &base_url).await {
        Ok(status) => Ok(status),
        Err(_) => {
            *state.target.lock().await = None;
            discover_clipbridge(app, state, None).await
        }
    }
}

#[tauri::command]
async fn get_items(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
) -> Result<Vec<ClipItem>, String> {
    let base_url = current_base_url(&app, &state).await?;
    let response = state
        .client
        .get(format!("{base_url}/api/items"))
        .timeout(Duration::from_millis(1800))
        .send()
        .await
        .map_err(|error| error.to_string())?;

    if !response.status().is_success() {
        return Err(format!("HTTP {}", response.status()));
    }

    Ok(response
        .json::<ItemsResponse>()
        .await
        .map_err(|error| error.to_string())?
        .items)
}

#[tauri::command]
async fn get_item(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    id: usize,
) -> Result<ClipItem, String> {
    let base_url = current_base_url(&app, &state).await?;
    fetch_item(&state.client, &base_url, id).await
}

#[tauri::command]
async fn send_text(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    text: String,
) -> Result<(), String> {
    let base_url = current_base_url(&app, &state).await?;
    let response = state
        .client
        .post(format!("{base_url}/api/text"))
        .timeout(Duration::from_millis(2200))
        .json(&serde_json::json!({ "text": text }))
        .send()
        .await
        .map_err(|error| error.to_string())?;

    let status = response.status();
    let body = response.json::<ApiOk>().await.ok();
    if !status.is_success() || body.as_ref().is_some_and(|ok| !ok.ok) {
        return Err(body
            .and_then(|ok| ok.error)
            .unwrap_or_else(|| format!("HTTP {status}")));
    }
    Ok(())
}

#[tauri::command]
async fn upload_file(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    path: String,
) -> Result<(), String> {
    let base_url = current_base_url(&app, &state).await?;
    let path_buf = PathBuf::from(path);
    let filename = path_buf
        .file_name()
        .and_then(|name| name.to_str())
        .ok_or_else(|| "Invalid file name".to_string())?
        .to_string();

    let file = File::open(&path_buf)
        .await
        .map_err(|error| format!("Could not open file: {error}"))?;
    let file_size = file
        .metadata()
        .await
        .map_err(|error| format!("Could not read file size: {error}"))?
        .len();
    let stream = ReaderStream::new(file);
    let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
    let part = multipart::Part::stream_with_length(reqwest::Body::wrap_stream(stream), file_size)
        .file_name(filename)
        .mime_str(mime.as_ref())
        .map_err(|error| error.to_string())?;
    let form = multipart::Form::new().part("file", part);

    let response = state
        .client
        .post(format!("{base_url}/api/upload"))
        .timeout(Duration::from_secs(120))
        .multipart(form)
        .send()
        .await
        .map_err(|error| error.to_string())?;

    let status = response.status();
    let body = response.json::<ApiOk>().await.ok();
    if !status.is_success() || body.as_ref().is_some_and(|ok| !ok.ok) {
        return Err(body
            .and_then(|ok| ok.message.or(ok.error))
            .unwrap_or_else(|| format!("HTTP {status}")));
    }
    Ok(())
}

#[cfg(windows)]
#[tauri::command]
fn clipboard_file_paths() -> Result<ClipboardFilesResult, String> {
    use clipboard_win::{formats, Clipboard, Getter};

    let _clipboard = Clipboard::new_attempts(10).map_err(|error| error.to_string())?;
    let mut paths = Vec::<PathBuf>::new();
    formats::FileList
        .read_clipboard(&mut paths)
        .map_err(|error| error.to_string())?;

    Ok(ClipboardFilesResult {
        paths: paths
            .into_iter()
            .filter(|path| path.is_file())
            .map(|path| path.to_string_lossy().to_string())
            .collect(),
    })
}

#[cfg(not(windows))]
#[tauri::command]
fn clipboard_file_paths() -> Result<ClipboardFilesResult, String> {
    Ok(ClipboardFilesResult { paths: Vec::new() })
}

#[tauri::command]
async fn save_clipboard_image(
    app: tauri::AppHandle,
    rgba: Vec<u8>,
    width: u32,
    height: u32,
) -> Result<String, String> {
    let expected = width as usize * height as usize * 4;
    if rgba.len() != expected {
        return Err("Clipboard image data is incomplete.".into());
    }

    let image_dir = app
        .path()
        .app_cache_dir()
        .unwrap_or_else(|_| std::env::temp_dir().join("ClipBridge"))
        .join("clipboard");
    tokio::fs::create_dir_all(&image_dir)
        .await
        .map_err(|error| error.to_string())?;

    let stamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or(0);
    let path = image_dir.join(format!("clipboard_{stamp}.png"));
    let path_for_blocking = path.clone();

    tokio::task::spawn_blocking(move || {
        image::save_buffer_with_format(
            &path_for_blocking,
            &rgba,
            width,
            height,
            image::ColorType::Rgba8,
            image::ImageFormat::Png,
        )
        .map_err(|error| error.to_string())
    })
    .await
    .map_err(|error| error.to_string())??;

    Ok(path.to_string_lossy().to_string())
}

async fn download_to_file(
    client: &Client,
    url: String,
    destination: &Path,
    timeout: Duration,
) -> Result<(), String> {
    let response = client
        .get(url)
        .timeout(timeout)
        .send()
        .await
        .map_err(|error| error.to_string())?;

    if !response.status().is_success() {
        return Err(format!("HTTP {}", response.status()));
    }

    if let Some(parent) = destination.parent() {
        tokio::fs::create_dir_all(parent)
            .await
            .map_err(|error| error.to_string())?;
    }

    let mut file = File::create(destination)
        .await
        .map_err(|error| format!("Could not create file: {error}"))?;
    let mut stream = response.bytes_stream();
    while let Some(chunk) = stream.next().await {
        let chunk = chunk.map_err(|error| error.to_string())?;
        file.write_all(&chunk)
            .await
            .map_err(|error| error.to_string())?;
    }
    file.flush().await.map_err(|error| error.to_string())?;
    Ok(())
}

fn sanitize_filename(name: &str) -> String {
    let mut clean = String::with_capacity(name.len());
    for ch in name.chars() {
        if matches!(ch, '<' | '>' | ':' | '"' | '/' | '\\' | '|' | '?' | '*') || ch.is_control() {
            clean.push('_');
        } else {
            clean.push(ch);
        }
    }
    let trimmed = clean.trim().trim_matches('.');
    if trimmed.is_empty() {
        "clipbridge-download.bin".into()
    } else {
        trimmed.to_string()
    }
}

fn available_path(path: PathBuf) -> PathBuf {
    if !path.exists() {
        return path;
    }

    let parent = path.parent().map(Path::to_path_buf).unwrap_or_default();
    let stem = path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("download");
    let extension = path.extension().and_then(|value| value.to_str());

    for index in 1..1000 {
        let filename = match extension {
            Some(ext) if !ext.is_empty() => format!("{stem} ({index}).{ext}"),
            _ => format!("{stem} ({index})"),
        };
        let candidate = parent.join(filename);
        if !candidate.exists() {
            return candidate;
        }
    }
    path
}

fn looks_like_pdf(item: &ClipItem) -> bool {
    item.mime
        .as_deref()
        .is_some_and(|mime| mime.eq_ignore_ascii_case("application/pdf"))
        || item.title.to_ascii_lowercase().ends_with(".pdf")
}

fn estimate_pdf_page_count(bytes: &[u8]) -> usize {
    let text = String::from_utf8_lossy(bytes);
    let count = text
        .match_indices("/Type")
        .filter(|(index, _)| {
            let tail = &text[*index..text.len().min(*index + 32)];
            tail.contains("/Page") && !tail.contains("/Pages")
        })
        .count();
    count.max(1)
}

async fn cached_item_file(
    app: &tauri::AppHandle,
    client: &Client,
    base_url: &str,
    item: &ClipItem,
    id: usize,
    subdir: &str,
) -> Result<PathBuf, String> {
    let preview_dir = app
        .path()
        .app_cache_dir()
        .unwrap_or_else(|_| std::env::temp_dir().join("ClipBridge"))
        .join(subdir);
    tokio::fs::create_dir_all(&preview_dir)
        .await
        .map_err(|error| error.to_string())?;

    let final_path = preview_dir.join(format!("{id}_{}", sanitize_filename(&item.title)));
    let needs_download = match (tokio::fs::metadata(&final_path).await, item.size) {
        (Ok(metadata), Some(expected)) => metadata.len() != expected,
        (Ok(_), None) => false,
        (Err(_), _) => true,
    };

    if needs_download {
        let partial_path = final_path.with_extension("part");
        let _ = tokio::fs::remove_file(&partial_path).await;
        download_to_file(
            client,
            format!("{base_url}/api/items/{id}/download"),
            &partial_path,
            Duration::from_secs(90),
        )
        .await?;

        if let Some(expected) = item.size {
            let downloaded = tokio::fs::metadata(&partial_path)
                .await
                .map_err(|error| error.to_string())?
                .len();
            if downloaded != expected {
                let _ = tokio::fs::remove_file(&partial_path).await;
                return Err(format!(
                    "Preview download incomplete: {downloaded} of {expected} bytes."
                ));
            }
        }

        let _ = tokio::fs::remove_file(&final_path).await;
        tokio::fs::rename(&partial_path, &final_path)
            .await
            .map_err(|error| error.to_string())?;
    }

    Ok(final_path)
}

#[tauri::command]
async fn download_item(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    id: usize,
    destination: Option<String>,
) -> Result<DownloadResult, String> {
    let base_url = current_base_url(&app, &state).await?;
    let item = fetch_item(&state.client, &base_url, id).await?;
    let filename = sanitize_filename(&item.title);
    let destination = match destination {
        Some(path) if !path.trim().is_empty() => PathBuf::from(path),
        _ => available_path(
            dirs::download_dir()
                .unwrap_or_else(std::env::temp_dir)
                .join(&filename),
        ),
    };

    let final_path = if destination.exists() {
        available_path(destination)
    } else {
        destination
    };
    download_to_file(
        &state.client,
        format!("{base_url}/api/items/{id}/download"),
        &final_path,
        Duration::from_secs(120),
    )
    .await?;

    Ok(DownloadResult {
        filename,
        path: final_path.to_string_lossy().to_string(),
    })
}

#[tauri::command]
async fn preview_image(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    id: usize,
) -> Result<PreviewResult, String> {
    let base_url = current_base_url(&app, &state).await?;
    let item = fetch_item(&state.client, &base_url, id).await?;
    if item.item_type != "image" {
        return Err("Selected item is not an image".into());
    }

    let size = item.size.unwrap_or(0);
    if size > 12 * 1024 * 1024 {
        return Err("Image is too large for inline preview".into());
    }

    let final_path = cached_item_file(&app, &state.client, &base_url, &item, id, "previews").await?;

    let bytes = tokio::fs::read(&final_path)
        .await
        .map_err(|error| format!("Could not read preview file: {error}"))?;
    let mime = item
        .mime
        .filter(|value| value.starts_with("image/"))
        .unwrap_or_else(|| {
            mime_guess::from_path(&final_path)
                .first_or_octet_stream()
                .to_string()
        });

    Ok(PreviewResult {
        data_url: format!("data:{mime};base64,{}", BASE64.encode(bytes)),
    })
}

#[tauri::command]
async fn preview_pdf(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    id: usize,
) -> Result<PdfPreviewResult, String> {
    let base_url = current_base_url(&app, &state).await?;
    let item = fetch_item(&state.client, &base_url, id).await?;
    if !looks_like_pdf(&item) {
        return Err("Selected item is not a PDF".into());
    }

    let final_path = cached_item_file(&app, &state.client, &base_url, &item, id, "pdf-previews").await?;
    let bytes = tokio::fs::read(&final_path)
        .await
        .map_err(|error| format!("Could not read PDF preview file: {error}"))?;

    Ok(PdfPreviewResult {
        path: final_path.to_string_lossy().to_string(),
        page_count: estimate_pdf_page_count(&bytes),
    })
}

#[tauri::command]
async fn delete_item(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    id: usize,
) -> Result<(), String> {
    let base_url = current_base_url(&app, &state).await?;
    let response = state
        .client
        .delete(format!("{base_url}/api/items/{id}"))
        .timeout(Duration::from_millis(2200))
        .send()
        .await
        .map_err(|error| error.to_string())?;

    let status = response.status();
    let body = response.json::<ApiOk>().await.ok();
    if !status.is_success() || body.as_ref().is_some_and(|ok| !ok.ok) {
        return Err(body
            .and_then(|ok| ok.error)
            .unwrap_or_else(|| format!("HTTP {status}")));
    }
    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let client = Client::builder()
        .user_agent("ClipBridge Desktop/0.1")
        .pool_max_idle_per_host(1)
        .build()
        .expect("failed to create HTTP client");

    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_clipboard_manager::init())
        .manage(AppState {
            client,
            target: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            discover_clipbridge,
            get_status,
            get_items,
            get_item,
            send_text,
            upload_file,
            download_item,
            preview_image,
            preview_pdf,
            clipboard_file_paths,
            save_clipboard_image,
            delete_item
        ])
        .run(tauri::generate_context!())
        .expect("error while running ClipBridge");
}

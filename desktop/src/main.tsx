import React, { useEffect, useMemo, useState } from "react";
import ReactDOM from "react-dom/client";
import { convertFileSrc, invoke } from "@tauri-apps/api/core";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import { open, save } from "@tauri-apps/plugin-dialog";
import { readImage, writeText } from "@tauri-apps/plugin-clipboard-manager";
import { openUrl } from "@tauri-apps/plugin-opener";
import {
  CheckCircle2,
  ChevronLeft,
  ChevronRight,
  Download,
  FileArchive,
  FileImage,
  FileText,
  Folder,
  FolderPlus,
  FolderUp,
  Globe,
  GripVertical,
  Link2,
  Loader2,
  RefreshCw,
  Send,
  Settings2,
  Trash2,
} from "lucide-react";
import logoFull from "./assets/logo-full.png";
import logoMark from "./assets/logo-mark.png";
import "./styles.css";

type DeviceStatus = {
  ok: boolean;
  device?: string;
  hostname?: string;
  version?: string;
  ap_ssid?: string;
  ap_ip?: string;
  sta_connected?: boolean;
  sta_ssid?: string;
  sta_ip?: string;
  sd?: boolean;
  battery_percent?: number;
  selected_id?: number;
  item_count?: number;
  base_url?: string;
};

type ClipItem = {
  id: number;
  type: "text" | "link" | "image" | "file";
  title: string;
  preview?: string;
  size?: number;
  selected?: boolean;
  mime?: string;
  text?: string;
};

type DownloadResult = {
  path: string;
  filename: string;
};

type PreviewResult = {
  data_url: string;
};

type PdfPreviewResult = {
  path: string;
  page_count: number;
};

type ClipboardFilesResult = {
  paths: string[];
};

type ConnectionState = "searching" | "connected" | "offline";
type MouseDrag = {
  key: string;
  x: number;
  y: number;
  startX: number;
  startY: number;
  active: boolean;
} | null;
type SentNotice = {
  id: number;
  text: string;
} | null;

const DEFAULT_FOLDERS = ["Images", "Documents", "Links"];
const UNFILED_FOLDER = "Unfiled";

const formatSize = (bytes?: number) => {
  if (!bytes) return "0 B";
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${bytes} B`;
};

const itemIcon = (item: ClipItem) => {
  if (item.type === "link") return <Link2 size={18} />;
  if (item.type === "image") return <FileImage size={18} />;
  if (item.type === "file") return <FileArchive size={18} />;
  return <FileText size={18} />;
};

const itemLabel = (item: ClipItem) => {
  if (item.type === "link") return "Link";
  if (item.type === "image") return "Image";
  if (item.type === "file") return item.title.split(".").pop()?.toUpperCase() || "File";
  return "Text";
};

const isPdfItem = (item: ClipItem) =>
  item.mime?.toLowerCase() === "application/pdf" || item.title.toLowerCase().endsWith(".pdf");

const linkWithScheme = (value: string) => {
  const trimmed = value.trim();
  if (/^[a-z][a-z\d+\-.]*:/i.test(trimmed)) return trimmed;
  return `https://${trimmed}`;
};

const itemIdentity = (item: ClipItem) =>
  `${item.type}|${item.title}|${item.size ?? 0}|${item.preview ?? ""}`;

const dragKeyFromEvent = (event: React.DragEvent) =>
  event.dataTransfer.getData("text/clipbridge-item") ||
  event.dataTransfer.getData("text/plain");

function App() {
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [connection, setConnection] = useState<ConnectionState>("searching");
  const [items, setItems] = useState<ClipItem[]>([]);
  const [selectedId, setSelectedId] = useState<number | null>(null);
  const [selectedDetail, setSelectedDetail] = useState<ClipItem | null>(null);
  const [previewPath, setPreviewPath] = useState<string | null>(null);
  const [pdfPreview, setPdfPreview] = useState<PdfPreviewResult | null>(null);
  const [pdfPage, setPdfPage] = useState(1);
  const [previewError, setPreviewError] = useState("");
  const [text, setText] = useState("");
  const [busy, setBusy] = useState<string | null>("discover");
  const [message, setMessage] = useState("");
  const [dropActive, setDropActive] = useState(false);
  const [advancedOpen, setAdvancedOpen] = useState(false);
  const [manualAddress, setManualAddress] = useState("");
  const [organizeOpen, setOrganizeOpen] = useState(false);
  const [folders, setFolders] = useState<string[]>(DEFAULT_FOLDERS);
  const [folderAssignments, setFolderAssignments] = useState<Record<string, string>>({});
  const [folderOrders, setFolderOrders] = useState<Record<string, string[]>>({});
  const [selectedFolder, setSelectedFolder] = useState(UNFILED_FOLDER);
  const [newFolderName, setNewFolderName] = useState("");
  const [draggedItemKey, setDraggedItemKey] = useState<string | null>(null);
  const [mouseDrag, setMouseDrag] = useState<MouseDrag>(null);
  const [suppressCardClick, setSuppressCardClick] = useState(false);
  const [sentNotice, setSentNotice] = useState<SentNotice>(null);
  const [selectionNonce, setSelectionNonce] = useState(0);

  const selected = useMemo(
    () => items.find((item) => item.id === selectedId) ?? null,
    [items, selectedId],
  );

  const itemFolder = (item: ClipItem) => folderAssignments[itemIdentity(item)] || UNFILED_FOLDER;

  const folderNames = useMemo(() => Array.from(new Set(folders)).filter(Boolean), [folders]);

  const itemByKey = useMemo(() => {
    const map = new Map<string, ClipItem>();
    items.forEach((item) => map.set(itemIdentity(item), item));
    return map;
  }, [items]);

  const driveItems = useMemo(() => {
    const base = items.filter((item) => itemFolder(item) === selectedFolder);
    const order = folderOrders[selectedFolder] ?? [];
    const orderIndex = new Map(order.map((key, index) => [key, index]));
    return [...base].sort((a, b) => {
      const aIndex = orderIndex.get(itemIdentity(a));
      const bIndex = orderIndex.get(itemIdentity(b));
      if (aIndex === undefined && bIndex === undefined) return a.id - b.id;
      if (aIndex === undefined) return 1;
      if (bIndex === undefined) return -1;
      return aIndex - bIndex;
    });
  }, [folderAssignments, folderOrders, items, selectedFolder]);

  const folderCount = (folder: string) => items.filter((item) => itemFolder(item) === folder).length;

  function showSentNotice(text = "Enviado a ClipBridge") {
    setSentNotice({ id: Date.now(), text });
  }

  function selectLatestItem(nextItems: ClipItem[]) {
    setSelectedDetail(null);
    setPreviewPath(null);
    setPdfPreview(null);
    setPdfPage(1);
    setPreviewError("");
    setSelectionNonce((value) => value + 1);
    return nextItems[0]?.id ?? null;
  }

  async function refreshItems(options?: { selectLatest?: boolean }) {
    const nextItems = await invoke<ClipItem[]>("get_items");
    let forcedSelectedId: number | null | undefined;

    if (options?.selectLatest) {
      forcedSelectedId = selectLatestItem(nextItems);
    }

    setItems(nextItems);
    setSelectedId((current) => {
      if (forcedSelectedId !== undefined) return forcedSelectedId;
      if (current !== null && nextItems.some((item) => item.id === current)) return current;
      return nextItems[0]?.id ?? null;
    });
    return nextItems;
  }

  async function discover(address?: string) {
    setBusy("discover");
    setConnection("searching");
    setMessage("");
    try {
      const nextStatus = await invoke<DeviceStatus>("discover_clipbridge", {
        manualAddress: address || null,
      });
      setStatus(nextStatus);
      setConnection("connected");
      await refreshItems();
    } catch (error) {
      setStatus(null);
      setItems([]);
      setSelectedId(null);
      setConnection("offline");
      setMessage(String(error));
    } finally {
      setBusy(null);
    }
  }

  async function pollStatus() {
    if (document.hidden || busy) return;
    try {
      const nextStatus = await invoke<DeviceStatus>("get_status");
      setStatus(nextStatus);
      setConnection("connected");
      const expectedCount = nextStatus.item_count ?? items.length;
      if (expectedCount !== items.length) await refreshItems();
    } catch {
      setConnection("offline");
    }
  }

  async function loadDetail(id: number) {
    try {
      const detail = await invoke<ClipItem>("get_item", { id });
      setSelectedDetail(detail);
      setPreviewPath(null);
      setPdfPreview(null);
      setPdfPage(1);
      setPreviewError("");
      if (detail.type === "image") {
        try {
          const preview = await invoke<PreviewResult>("preview_image", { id });
          setPreviewPath(preview.data_url);
        } catch (error) {
          setPreviewError(String(error));
        }
      } else if (detail.type === "file" && isPdfItem(detail)) {
        try {
          const preview = await invoke<PdfPreviewResult>("preview_pdf", { id });
          setPdfPreview(preview);
        } catch (error) {
          setPreviewError(String(error));
        }
      }
    } catch (error) {
      setMessage(String(error));
    }
  }

  async function sendText() {
    const value = text.trim();
    if (!value) return;
    setBusy("send");
    try {
      await invoke("send_text", { text: value });
      setText("");
      setMessage("Sent to ClipBridge.");
      showSentNotice("Texto enviado");
      await refreshItems({ selectLatest: true });
    } catch (error) {
      setMessage(String(error));
    } finally {
      setBusy(null);
    }
  }

  async function uploadPath(path: string) {
    setBusy("upload");
    try {
      await invoke("upload_file", { path });
      setMessage("Uploaded to ClipBridge.");
      showSentNotice("Archivo enviado");
      await refreshItems({ selectLatest: true });
    } catch (error) {
      setMessage(String(error));
    } finally {
      setBusy(null);
      setDropActive(false);
    }
  }

  async function uploadPaths(paths: string[]) {
    if (!paths.length) return;
    setBusy("upload");
    try {
      for (const path of paths) {
        await invoke("upload_file", { path });
      }
      setMessage(paths.length === 1 ? "Uploaded to ClipBridge." : `Uploaded ${paths.length} files to ClipBridge.`);
      showSentNotice(paths.length === 1 ? "Archivo enviado" : `${paths.length} archivos enviados`);
      await refreshItems({ selectLatest: true });
    } catch (error) {
      setMessage(String(error));
    } finally {
      setBusy(null);
      setDropActive(false);
    }
  }

  async function chooseFile() {
    const picked = await open({ multiple: false, directory: false });
    const path = Array.isArray(picked) ? picked[0] : picked;
    if (typeof path === "string") await uploadPath(path);
  }

  async function pasteClipboardFilesOrImage(tryImage = true) {
    try {
      const files = await invoke<ClipboardFilesResult>("clipboard_file_paths");
      if (files.paths.length) {
        await uploadPaths(files.paths);
        return true;
      }
    } catch {
      // Text-only clipboards do not expose a file list.
    }

    if (!tryImage) return false;

    try {
      const image = await readImage();
      const [size, rgba] = await Promise.all([image.size(), image.rgba()]);
      const path = await invoke<string>("save_clipboard_image", {
        rgba: Array.from(rgba),
        width: size.width,
        height: size.height,
      });
      await uploadPath(path);
      return true;
    } catch {
      return false;
    }
  }

  async function handleTextPaste(event: React.ClipboardEvent<HTMLTextAreaElement>) {
    const pastedText = event.clipboardData.getData("text/plain");
    const hasWebFiles = event.clipboardData.files.length > 0;
    const selectionStart = event.currentTarget.selectionStart;
    const selectionEnd = event.currentTarget.selectionEnd;

    event.preventDefault();
    const uploaded = await pasteClipboardFilesOrImage(!pastedText || hasWebFiles);
    if (!uploaded && pastedText) {
      setText((current) => `${current.slice(0, selectionStart)}${pastedText}${current.slice(selectionEnd)}`);
    } else if (!uploaded) {
      setMessage("Clipboard does not contain a file or image.");
    }
  }

  async function copyItem(item: ClipItem) {
    try {
      const detail = selectedDetail?.id === item.id
        ? selectedDetail
        : await invoke<ClipItem>("get_item", { id: item.id });
      const value = detail.text ?? detail.preview ?? item.preview ?? item.title;
      if (!value.trim()) {
        setMessage("Nothing to copy.");
        return;
      }
      await writeText(value);
      setSelectedId(item.id);
      setSelectedDetail(detail);
      setMessage("Copied to Windows clipboard.");
    } catch (error) {
      setMessage(String(error));
    }
  }

  async function copySelected() {
    if (!selected) return;
    await copyItem(selected);
  }

  async function openSelectedLink() {
    if (!selected) return;
    const detail = selectedDetail?.id === selected.id ? selectedDetail : await invoke<ClipItem>("get_item", { id: selected.id });
    await openUrl(linkWithScheme(detail.text ?? detail.preview ?? ""));
  }

  async function downloadSelected() {
    if (!selected) return;
    const destination = await save({ defaultPath: selected.title || "clipbridge-download" });
    if (!destination) return;
    setBusy("download");
    try {
      const result = await invoke<DownloadResult>("download_item", {
        id: selected.id,
        destination,
      });
      setMessage(`Saved ${result.filename}.`);
    } catch (error) {
      setMessage(String(error));
    } finally {
      setBusy(null);
    }
  }

  async function deleteSelected() {
    if (!selected) return;
    setBusy("delete");
    try {
      await invoke("delete_item", { id: selected.id });
      setMessage("Deleted.");
      setSelectedDetail(null);
      await refreshItems();
    } catch (error) {
      setMessage(String(error));
    } finally {
      setBusy(null);
    }
  }

  function addFolder() {
    const name = newFolderName.trim();
    if (!name || name.toLowerCase() === "all" || name === UNFILED_FOLDER) return;
    setFolders((current) => Array.from(new Set([...current, name])));
    setSelectedFolder(name);
    setNewFolderName("");
  }

  function assignItemToFolder(item: ClipItem, folder: string) {
    const key = itemIdentity(item);
    setFolderAssignments((current) => ({
      ...current,
      [key]: folder,
    }));
    setFolderOrders((current) => {
      const next: Record<string, string[]> = {};
      Object.entries(current).forEach(([name, order]) => {
        next[name] = order.filter((value) => value !== key);
      });
      next[folder] = [...(next[folder] ?? []), key];
      return next;
    });
    setMessage(`Moved to ${folder}.`);
  }

  function assignSelectedToFolder(folder: string) {
    if (!selected) return;
    assignItemToFolder(selected, folder);
  }

  function dropItemOnFolder(folder: string, key: string | null) {
    if (!key) return;
    const item = itemByKey.get(key);
    if (!item) return;
    assignItemToFolder(item, folder);
    setSelectedFolder(folder);
    setSelectedId(item.id);
    setDraggedItemKey(null);
  }

  function handleFolderDrop(event: React.DragEvent, folder: string) {
    event.preventDefault();
    event.stopPropagation();
    dropItemOnFolder(folder, dragKeyFromEvent(event) || draggedItemKey);
  }

  function finishMouseDrag(clientX: number, clientY: number) {
    if (!mouseDrag) return;

    const key = mouseDrag.key;
    const target = document.elementFromPoint(clientX, clientY) as HTMLElement | null;
    const folderDrop = target?.closest<HTMLElement>("[data-folder-drop]");
    const cardDrop = target?.closest<HTMLElement>("[data-drive-card]");
    const filesDrop = target?.closest<HTMLElement>("[data-current-folder-drop]");

    if (folderDrop?.dataset.folderDrop) {
      dropItemOnFolder(folderDrop.dataset.folderDrop, key);
    } else if (cardDrop?.dataset.driveCard) {
      reorderDriveItem(cardDrop.dataset.driveCard, key);
    } else if (filesDrop) {
      dropItemIntoCurrentFolder(key);
    }

    setSuppressCardClick(mouseDrag.active);
    setMouseDrag(null);
    window.setTimeout(() => setSuppressCardClick(false), 0);
  }

  function dropItemIntoCurrentFolder(key: string | null) {
    if (!key) return;
    const item = itemByKey.get(key);
    if (!item) return;
    assignItemToFolder(item, selectedFolder);
    setDraggedItemKey(null);
  }

  function handleCurrentFolderDrop(event: React.DragEvent) {
    event.preventDefault();
    dropItemIntoCurrentFolder(dragKeyFromEvent(event) || draggedItemKey);
  }

  function reorderDriveItem(targetKey: string, draggedKey = draggedItemKey) {
    if (!draggedKey || draggedKey === targetKey) return;
    setFolderOrders((current) => {
      const existing = current[selectedFolder] ?? driveItems.map(itemIdentity);
      const withoutDragged = existing.filter((key) => key !== draggedKey);
      const targetIndex = withoutDragged.indexOf(targetKey);
      const insertAt = targetIndex >= 0 ? targetIndex : withoutDragged.length;
      const nextOrder = [...withoutDragged];
      nextOrder.splice(insertAt, 0, draggedKey);
      return {
        ...current,
        [selectedFolder]: nextOrder,
      };
    });
    setDraggedItemKey(null);
  }

  useEffect(() => {
    void discover();
  }, []);

  useEffect(() => {
    try {
      const raw = window.localStorage.getItem("clipbridge-folders-v1");
      if (!raw) return;
      const saved = JSON.parse(raw) as {
        folders?: string[];
        assignments?: Record<string, string>;
        orders?: Record<string, string[]>;
      };
      if (Array.isArray(saved.folders)) setFolders(saved.folders);
      if (saved.assignments && typeof saved.assignments === "object") {
        setFolderAssignments(saved.assignments);
      }
      if (saved.orders && typeof saved.orders === "object") {
        setFolderOrders(saved.orders);
      }
    } catch {
      setFolders(DEFAULT_FOLDERS);
      setFolderAssignments({});
      setFolderOrders({});
    }
  }, []);

  useEffect(() => {
    window.localStorage.setItem(
      "clipbridge-folders-v1",
      JSON.stringify({ folders, assignments: folderAssignments, orders: folderOrders }),
    );
  }, [folderAssignments, folderOrders, folders]);

  useEffect(() => {
    const timer = window.setInterval(() => void pollStatus(), 3000);
    return () => window.clearInterval(timer);
  });

  useEffect(() => {
    if (selectedId !== null) void loadDetail(selectedId);
  }, [selectedId, selectionNonce]);

  useEffect(() => {
    if (!sentNotice) return;
    const timer = window.setTimeout(() => setSentNotice(null), 3200);
    return () => window.clearTimeout(timer);
  }, [sentNotice]);

  useEffect(() => {
    if (!organizeOpen) return;
    if (selectedId !== null && driveItems.some((item) => item.id === selectedId)) return;
    setSelectedId(driveItems[0]?.id ?? null);
  }, [driveItems, organizeOpen, selectedFolder, selectedId]);

  useEffect(() => {
    if (!mouseDrag) return;

    const onMouseMove = (event: MouseEvent) => {
      event.preventDefault();
      setMouseDrag((current) => {
        if (!current) return current;
        const distance = Math.hypot(event.clientX - current.startX, event.clientY - current.startY);
        return {
          ...current,
          x: event.clientX,
          y: event.clientY,
          active: current.active || distance > 4,
        };
      });
    };

    const onMouseUp = (event: MouseEvent) => {
      event.preventDefault();
      finishMouseDrag(event.clientX, event.clientY);
    };

    window.addEventListener("mousemove", onMouseMove);
    window.addEventListener("mouseup", onMouseUp, { once: true });
    return () => {
      window.removeEventListener("mousemove", onMouseMove);
      window.removeEventListener("mouseup", onMouseUp);
    };
  }, [mouseDrag, driveItems, selectedFolder]);

  useEffect(() => {
    let unlisten: (() => void) | undefined;
    void getCurrentWebview().onDragDropEvent((event) => {
      if (event.payload.type === "over") {
        setDropActive(true);
      } else if (event.payload.type === "drop") {
        const [path] = event.payload.paths;
        if (path) void uploadPath(path);
      } else {
        setDropActive(false);
      }
    }).then((dispose) => {
      unlisten = dispose;
    });
    return () => unlisten?.();
  }, []);

  const statusText =
    connection === "connected"
      ? "Connected"
      : connection === "searching"
        ? "Searching..."
        : "Not found";
  const pdfUrl = pdfPreview
    ? `${convertFileSrc(pdfPreview.path)}#page=${pdfPage}&toolbar=0&navpanes=0&scrollbar=0&view=FitH`
    : "";

  return (
    <main className={`app ${mouseDrag?.active ? "dragging-drive-item" : ""}`}>
      <header className="topbar">
        <div className="brand">
          <img src={logoMark} alt="" />
          <strong>ClipBridge</strong>
          <span>by Aleix Ferrer</span>
        </div>
        <div className="top-actions">
          <button
            className={`toolbar-button ${organizeOpen ? "active" : ""}`}
            onClick={() => setOrganizeOpen((open) => !open)}
          >
            <Folder size={15} /> Drive
          </button>
          <div className={`status ${connection}`}>
            <span className="dot" />
            <span>{statusText}</span>
          </div>
          {connection === "connected" && (
            <span className="device-chip">
              ClipBridge · {status?.sta_ip || status?.ap_ip || status?.base_url?.replace(/^https?:\/\//, "")}
            </span>
          )}
          <button className="icon-button" title="Refresh" onClick={() => void discover(manualAddress)}>
            {busy === "discover" ? <Loader2 className="spin" size={17} /> : <RefreshCw size={17} />}
          </button>
        </div>
      </header>

      {connection === "offline" ? (
        <section className="offline">
          <img className="offline-logo" src={logoFull} alt="ClipBridge" />
          <h1>ClipBridge not found</h1>
          <p>Connect to the ClipBridge Wi-Fi network or make sure ClipBridge and this PC are on the same local network.</p>
          <button className="primary" onClick={() => void discover(manualAddress)}>Try again</button>
          <span>Direct connection: 192.168.4.1</span>
          {message && <code>{message}</code>}
        </section>
      ) : organizeOpen ? (
        <section className="drive-workspace">
          <div className="drive-shell">
            <div className="drive-searchbar">
              <Folder size={18} />
              <span>ClipBridge Drive</span>
              <button
                className={`drive-pill ${selectedFolder === UNFILED_FOLDER ? "active" : ""}`}
                data-folder-drop={UNFILED_FOLDER}
                onClick={() => setSelectedFolder(UNFILED_FOLDER)}
                onDragOver={(event) => {
                  event.preventDefault();
                  event.dataTransfer.dropEffect = "move";
                }}
                onDrop={(event) => handleFolderDrop(event, UNFILED_FOLDER)}
              >
                Unfiled · {folderCount(UNFILED_FOLDER)}
              </button>
              <button className="icon-button small" title="Refresh items" onClick={() => void refreshItems()}>
                <RefreshCw size={15} />
              </button>
            </div>

            <div className="drive-title-row">
              <h2>Drive</h2>
              <div className="folder-create drive-create">
                <input
                  value={newFolderName}
                  onChange={(event) => setNewFolderName(event.target.value)}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") addFolder();
                  }}
                  placeholder="New folder"
                />
                <button className="icon-button small" title="Create folder" onClick={addFolder}>
                  <FolderPlus size={15} />
                </button>
              </div>
            </div>

            <section className="drive-section">
              <div className="drive-section-title">
                <span>Suggested folders</span>
              </div>
              <div className="drive-folder-grid">
                {folderNames.map((folder) => (
                  <button
                    key={folder}
                    className={`drive-folder-card ${selectedFolder === folder ? "active" : ""}`}
                    data-folder-drop={folder}
                    onClick={() => setSelectedFolder(folder)}
                    onDragOver={(event) => {
                      event.preventDefault();
                      event.dataTransfer.dropEffect = "move";
                    }}
                    onDrop={(event) => handleFolderDrop(event, folder)}
                  >
                    <Folder size={20} />
                    <strong>{folder}</strong>
                    <small>{folderCount(folder)} item{folderCount(folder) === 1 ? "" : "s"}</small>
                  </button>
                ))}
              </div>
            </section>

            <section
              className="drive-section drive-files-section"
              data-current-folder-drop="true"
              onDragOver={(event) => {
                event.preventDefault();
                event.dataTransfer.dropEffect = "move";
              }}
              onDrop={handleCurrentFolderDrop}
            >
              <div className="drive-section-title">
                <span>{selectedFolder === UNFILED_FOLDER ? "Unfiled files" : selectedFolder}</span>
                <small>{driveItems.length} item{driveItems.length === 1 ? "" : "s"}</small>
              </div>
              <div className="drive-file-grid">
                {driveItems.map((item) => {
                  const key = itemIdentity(item);
                  return (
                    <article
                      key={`${key}-${selectionNonce}`}
                      className={`drive-file-card ${selectedId === item.id ? "active" : ""} ${mouseDrag?.key === key && mouseDrag.active ? "dragging" : ""}`}
                      data-drive-card={key}
                      onClick={() => {
                        if (!suppressCardClick) setSelectedId(item.id);
                      }}
                      onMouseDown={(event) => {
                        if (event.button !== 0) return;
                        event.preventDefault();
                        setDraggedItemKey(key);
                        setMouseDrag({
                          key,
                          x: event.clientX,
                          y: event.clientY,
                          startX: event.clientX,
                          startY: event.clientY,
                          active: false,
                        });
                      }}
                      onDragStart={(event) => {
                        event.dataTransfer.effectAllowed = "move";
                        event.dataTransfer.setData("text/clipbridge-item", key);
                        event.dataTransfer.setData("text/plain", key);
                        setDraggedItemKey(key);
                      }}
                      onDragOver={(event) => event.preventDefault()}
                      onDrop={(event) => {
                        event.preventDefault();
                        event.stopPropagation();
                        reorderDriveItem(key, dragKeyFromEvent(event) || draggedItemKey);
                      }}
                      onDragEnd={() => setDraggedItemKey(null)}
                    >
                      <div className="drive-file-top">
                        <span className={`tiny-type ${item.type}`}>{itemIcon(item)}</span>
                        <strong>{item.title || item.preview || itemLabel(item)}</strong>
                        <GripVertical size={15} />
                      </div>
                      <div className="drive-file-preview">
                        <span className={`large-type ${item.type}`}>{itemIcon(item)}</span>
                      </div>
                      <div className="drive-file-meta">
                        <span>{item.type === "text" || item.type === "link" ? itemLabel(item) : formatSize(item.size)}</span>
                        <span>{itemFolder(item)}</span>
                      </div>
                    </article>
                  );
                })}
                {!driveItems.length && (
                  <div className="drive-empty">Drop files here</div>
                )}
              </div>
            </section>

            {selected && (
              <div className="drive-action-bar">
                <strong>{selected.title || itemLabel(selected)}</strong>
                <select
                  value={itemFolder(selected)}
                  onChange={(event) => assignSelectedToFolder(event.target.value)}
                >
                  {[UNFILED_FOLDER, ...folderNames].map((folder) => (
                    <option key={folder} value={folder}>
                      {folder}
                    </option>
                  ))}
                </select>
                {(selected.type === "text" || selected.type === "link") && (
                  <button
                    onClick={(event) => {
                      event.stopPropagation();
                      void copyItem(selected);
                    }}
                  >
                    <FileText size={16} /> Copy
                  </button>
                )}
                {(selected.type === "image" || selected.type === "file") && (
                  <button onClick={() => void downloadSelected()}>
                    <Download size={16} /> Download
                  </button>
                )}
              </div>
            )}
            {mouseDrag?.active && (
              <div
                className="drag-float"
                style={{ left: mouseDrag.x + 12, top: mouseDrag.y + 12 }}
              >
                {itemByKey.get(mouseDrag.key)?.title || "Moving item"}
              </div>
            )}
          </div>
        </section>
      ) : (
        <section className="workspace">
          <aside className="items">
            <div className="section-title">
              <span>{items.length} item{items.length === 1 ? "" : "s"}</span>
              <button className="icon-button small" title="Refresh items" onClick={() => void refreshItems()}>
                <RefreshCw size={15} />
              </button>
            </div>
            <div className="item-list">
              {items.map((item) => (
                <button
                  key={`${item.id}-${selectionNonce}`}
                  className={`item-row ${selectedId === item.id ? "active" : ""}`}
                  onClick={() => setSelectedId(item.id)}
                >
                  <span className={`type-icon ${item.type}`}>{itemIcon(item)}</span>
                  <span className="item-copy">
                    <strong>{item.title || item.preview || itemLabel(item)}</strong>
                    <small>{item.preview || item.mime || itemLabel(item)}</small>
                  </span>
                  <span className="item-meta">{item.type === "text" || item.type === "link" ? itemLabel(item) : formatSize(item.size)}</span>
                </button>
              ))}
              {!items.length && (
                <div className="empty-list">
                  {busy === "discover" ? "Searching..." : "No stored items"}
                </div>
              )}
            </div>
          </aside>

          <section className="detail">
            {selected ? (
              <>
                <div className="detail-head">
                  <span className={`type-icon ${selected.type}`}>{itemIcon(selected)}</span>
                  <div>
                    <h2>{selected.title || itemLabel(selected)}</h2>
                    <p>{itemLabel(selected)} · {formatSize(selected.size)}</p>
                  </div>
                </div>

                <div className="preview">
                  {selected.type === "text" || selected.type === "link" ? (
                    <pre>{selectedDetail?.text ?? selected.preview}</pre>
                  ) : selected.type === "image" && previewPath ? (
                    <div className="image-preview">
                      <img src={previewPath} alt={selected.title} />
                    </div>
                  ) : selected.type === "file" && isPdfItem(selected) && pdfPreview ? (
                    <div className="pdf-preview">
                      <div className="pdf-toolbar">
                        <button
                          className="icon-button small"
                          title="Previous page"
                          disabled={pdfPage <= 1}
                          onClick={() => setPdfPage((page) => Math.max(1, page - 1))}
                        >
                          <ChevronLeft size={15} />
                        </button>
                        <span>Page {pdfPage} / {pdfPreview.page_count}</span>
                        <button
                          className="icon-button small"
                          title="Next page"
                          disabled={pdfPage >= pdfPreview.page_count}
                          onClick={() => setPdfPage((page) => Math.min(pdfPreview.page_count, page + 1))}
                        >
                          <ChevronRight size={15} />
                        </button>
                      </div>
                      <iframe
                        key={`${pdfPreview.path}-${pdfPage}`}
                        title={selected.title}
                        src={pdfUrl}
                      />
                    </div>
                  ) : (
                    <div className="file-preview">
                      {selected.type === "image" ? <FileImage size={38} /> : <FileArchive size={38} />}
                      <strong>{selected.title}</strong>
                      <span>{previewError || selected.mime || formatSize(selected.size)}</span>
                    </div>
                  )}
                </div>

                <div className="context-actions">
                  {(selected.type === "text" || selected.type === "link") && (
                    <button className="primary" onClick={() => void copySelected()}>
                      <FileText size={16} /> Copy
                    </button>
                  )}
                  {selected.type === "link" && (
                    <button onClick={() => void openSelectedLink()}>
                      <Globe size={16} /> Open
                    </button>
                  )}
                  {(selected.type === "image" || selected.type === "file") && (
                    <button className="primary" onClick={() => void downloadSelected()}>
                      <Download size={16} /> Download
                    </button>
                  )}
                  <button className="danger" onClick={() => void deleteSelected()}>
                    <Trash2 size={16} /> Delete
                  </button>
                </div>
              </>
            ) : (
              <div className="empty-detail">No item selected</div>
            )}
          </section>

          <aside className={`send ${dropActive ? "drop" : ""}`}>
            <div className="section-title">
              <span>Send to ClipBridge</span>
            </div>
            <textarea
              value={text}
              onChange={(event) => setText(event.target.value)}
              onPaste={(event) => void handleTextPaste(event)}
              placeholder="Paste something into ClipBridge..."
            />
            <button className="primary" onClick={() => void sendText()} disabled={!text.trim() || Boolean(busy)}>
              <Send size={16} /> Send to ClipBridge
            </button>
            <button onClick={() => void chooseFile()} disabled={Boolean(busy)}>
              <FolderUp size={16} /> Choose file
            </button>
            <div className="drop-target">Drop files here</div>
            <button className="advanced-toggle" onClick={() => setAdvancedOpen((open) => !open)}>
              <Settings2 size={15} /> Advanced
            </button>
            {advancedOpen && (
              <div className="advanced">
                <input
                  value={manualAddress}
                  onChange={(event) => setManualAddress(event.target.value)}
                  placeholder="http://192.168.1.42"
                />
                <button onClick={() => void discover(manualAddress)}>Connect</button>
              </div>
            )}
            {message && <div className="message">{message}</div>}
          </aside>
        </section>
      )}
      {sentNotice && (
        <div className="sent-notice" key={sentNotice.id}>
          <CheckCircle2 size={18} />
          <span>{sentNotice.text}</span>
        </div>
      )}
    </main>
  );
}

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);

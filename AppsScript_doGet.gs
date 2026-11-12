// Google Apps Script — paste into Extensions > Apps Script for your Sheet
// Deploy as Web App: Execute as "Me", Access "Anyone with the link"
// The deployment URL is what you put in the ESP32 sketch as APPS_SCRIPT_URL

const SHEET_NAME = "Members"; // change if your tab is named differently

// Shared secret so random visitors can't spam new rows into the sheet.
// Must match REGISTER_TOKEN in the ESP32 sketch. Change it to your own value.
const REGISTER_TOKEN = "change-me-fablab";

function doGet(e) {
  const uid = (e.parameter.uid || "").trim().toUpperCase();
  const mode = (e.parameter.mode || "").trim().toLowerCase();

  const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(SHEET_NAME);
  const data = sheet.getDataRange().getValues();
  // Expected columns (row 0 = header): UID, Name, Onboarding, Laser, CNC, Bambu
  const headers = data[0];
  const rows = data.slice(1);

  // Bulk export: ?mode=all -> returns every member in one response,
  // used by the ESP32 to build/refresh its local cache.
  if (mode === "all") {
    const members = rows
      .filter(row => String(row[0]).trim() !== "") // skip blank rows
      .map(row => ({
        uid: String(row[0]).trim().toUpperCase(),
        name: row[1],
        onboarding: Boolean(row[2]),
        laser: Boolean(row[3]),
        cnc: Boolean(row[4]),
        bambu: Boolean(row[5])
      }));
    return jsonResponse({ members: members });
  }

  // Register: ?mode=register&uid=XX&token=... -> appends a placeholder row
  // (name "NEW USER", every permission FALSE) if the UID isn't in the sheet yet.
  if (mode === "register") {
    if ((e.parameter.token || "") !== REGISTER_TOKEN) {
      return jsonResponse({ ok: false, error: "bad token" });
    }
    if (!uid) {
      return jsonResponse({ ok: false, error: "missing uid param" });
    }

    // Lock so two quick taps can't both append the same UID
    const lock = LockService.getScriptLock();
    lock.waitLock(10000);
    try {
      const existing = sheet.getDataRange().getValues().slice(1);
      for (const row of existing) {
        if (String(row[0]).trim().toUpperCase() === uid) {
          return jsonResponse({ ok: true, created: false, uid: uid });
        }
      }
      const newRow = sheet.getLastRow() + 1;
      sheet.getRange(newRow, 1).setNumberFormat("@"); // keep UID as plain text, not a time/number
      sheet.getRange(newRow, 1, 1, 6).setValues([[uid, "NEW USER", false, false, false, false]]);
      return jsonResponse({ ok: true, created: true, uid: uid });
    } finally {
      lock.releaseLock();
    }
  }

  if (!uid) {
    return jsonResponse({ found: false, error: "missing uid param" });
  }

  for (const row of rows) {
    const rowUid = String(row[0]).trim().toUpperCase();
    if (rowUid === uid) {
      return jsonResponse({
        found: true,
        uid: rowUid,
        name: row[1],
        onboarding: Boolean(row[2]),
        laser: Boolean(row[3]),
        cnc: Boolean(row[4]),
        bambu: Boolean(row[5])
      });
    }
  }

  // UID not found -> new/unassigned tag
  return jsonResponse({ found: false, uid: uid });
}

function jsonResponse(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}

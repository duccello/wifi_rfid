// Google Apps Script — paste into Extensions > Apps Script for your Sheet
// Deploy as Web App: Execute as "Me", Access "Anyone with the link"
// The deployment URL is what you put in the ESP32 sketch as APPS_SCRIPT_URL

const SHEET_NAME = "Members"; // change if your tab is named differently

function doGet(e) {
  const uid = (e.parameter.uid || "").trim().toUpperCase();

  const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(SHEET_NAME);
  const data = sheet.getDataRange().getValues();
  // Expected columns (row 0 = header): UID, Name, Onboarding, Laser, CNC, Bambu
  const headers = data[0];
  const rows = data.slice(1);

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

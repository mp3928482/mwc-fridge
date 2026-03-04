// ============================================================================
//  MWC Fridge Monitor — Google Apps Script
//  Deploy as: Web App → Execute as: Me → Who has access: Anyone
// ============================================================================

// ── Configuration ─────────────────────────────────────────────────────────────
var SPREADSHEET_ID  = "YOUR_SPREADSHEET_ID";   // From the sheet URL
var ALERT_EMAIL     = "your@email.com";         // Where to send temp alerts
var SHEET_TAB_NAME  = "Readings";               // Tab name in the spreadsheet

// Cooldown: don't send another alert for the same fridge within this many minutes
var ALERT_COOLDOWN_MIN = 60;

// ── doGet — called by ESP32 ───────────────────────────────────────────────────
function doGet(e) {
  try {
    var params  = e.parameter;
    var fridge  = params.fridge  || "Unknown";
    var tempF   = parseFloat(params.tempF  || "0");
    var tempC   = parseFloat(params.tempC  || "0");
    var status  = params.status  || "OK";
    var version = params.version || "?";
    var ts      = new Date();

    // ── Write row to sheet ──────────────────────────────────────────────────
    var ss    = SpreadsheetApp.openById(SPREADSHEET_ID);
    var sheet = ss.getSheetByName(SHEET_TAB_NAME);

    // Auto-create tab with headers if it doesn't exist yet
    if (!sheet) {
      sheet = ss.insertSheet(SHEET_TAB_NAME);
      sheet.appendRow(["Timestamp", "Fridge ID", "Temp (°F)", "Temp (°C)", "Status", "Firmware"]);
      sheet.setFrozenRows(1);

      // Format header row
      var header = sheet.getRange(1, 1, 1, 6);
      header.setBackground("#1a73e8");
      header.setFontColor("#ffffff");
      header.setFontWeight("bold");
    }

    sheet.appendRow([ts, fridge, tempF, tempC, status, version]);

    // ── Colour-code the status cell ─────────────────────────────────────────
    var lastRow   = sheet.getLastRow();
    var statusCell = sheet.getRange(lastRow, 5);   // column E = Status
    if (status === "OK") {
      statusCell.setBackground("#d9ead3");   // light green
    } else if (status.startsWith("ALERT")) {
      statusCell.setBackground("#f4cccc");   // light red
    } else {
      statusCell.setBackground("#fff2cc");   // light yellow (SENSOR_ERROR etc)
    }

    // ── Send alert email if out of range ────────────────────────────────────
    if (status.startsWith("ALERT") || status === "SENSOR_ERROR") {
      maybeSendAlert(fridge, tempF, tempC, status, ts);
    }

    return ContentService.createTextOutput("OK");

  } catch (err) {
    Logger.log("doGet error: " + err.toString());
    return ContentService.createTextOutput("ERROR: " + err.toString());
  }
}

// ── Alert email with cooldown ─────────────────────────────────────────────────
function maybeSendAlert(fridge, tempF, tempC, status, ts) {
  var props     = PropertiesService.getScriptProperties();
  var cooldownKey = "last_alert_" + fridge;
  var lastAlert   = props.getProperty(cooldownKey);
  var now         = new Date().getTime();

  // Check cooldown
  if (lastAlert) {
    var elapsed = (now - parseInt(lastAlert)) / 60000;   // minutes
    if (elapsed < ALERT_COOLDOWN_MIN) {
      Logger.log("Alert suppressed for " + fridge + " (cooldown: " + elapsed.toFixed(1) + " min elapsed)");
      return;
    }
  }

  // Build email
  var subject, body;

  if (status === "SENSOR_ERROR") {
    subject = "⚠️ SENSOR ERROR — " + fridge;
    body    = "The temperature sensor on " + fridge + " is not responding.\n\n"
            + "Time: " + ts.toString() + "\n"
            + "Please check the probe wiring.\n";
  } else {
    var direction = (status === "ALERT_HIGH") ? "HIGH ↑" : "LOW ↓";
    subject = "🌡️ Temp Alert " + direction + " — " + fridge;
    body    = "Temperature out of range on " + fridge + "\n\n"
            + "Current:  " + tempF.toFixed(1) + " °F  /  " + tempC.toFixed(1) + " °C\n"
            + "Status:   " + status + "\n"
            + "Time:     " + ts.toString() + "\n\n"
            + "Please check the refrigerator immediately.\n";
  }

  MailApp.sendEmail(ALERT_EMAIL, subject, body);
  Logger.log("Alert sent for " + fridge + ": " + status);

  // Update cooldown timestamp
  props.setProperty(cooldownKey, now.toString());
}

// ── Utility: manually clear alert cooldown for a fridge (run from editor) ────
function clearAlertCooldown() {
  var props = PropertiesService.getScriptProperties();
  props.deleteAllProperties();
  Logger.log("All alert cooldowns cleared.");
}

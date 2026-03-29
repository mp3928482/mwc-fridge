// ============================================================================
//  MWC Fridge Monitor — Google Apps Script
//  Deploy as: Web App → Execute as: Me → Who has access: Anyone
// ============================================================================

// ── Configuration ─────────────────────────────────────────────────────────────
var SPREADSHEET_ID     = "YOUR_SPREADSHEET_ID";   // From the sheet URL
var ALERT_EMAIL        = "your@email.com";         // Where to send temp alerts
var ALERT_COOLDOWN_MIN = 60;                       // Minutes between alert emails per fridge


// ── Get or create the sheet tab for the current month ────────────────────────
// Tab name format: "Mar 2026", "Apr 2026", etc.
function getMonthlySheet(ss) {
  var now       = new Date();
  var months    = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
  var tabName   = months[now.getMonth()] + " " + now.getFullYear();

  var sheet = ss.getSheetByName(tabName);

  // Auto-create tab with headers if this is the first reading of the month
  if (!sheet) {
    sheet = ss.insertSheet(tabName);

    // Add header row
    sheet.appendRow(["Timestamp", "Fridge ID", "Temp (°F)", "Temp (°C)", "Status", "Firmware", "Acknowledged By / Action"]);
    sheet.setFrozenRows(1);

    // Format header row
    var header = sheet.getRange(1, 1, 1, 7);
    header.setBackground("#1a73e8");
    header.setFontColor("#ffffff");
    header.setFontWeight("bold");

    // Set column widths for readability
    sheet.setColumnWidth(1, 160);  // Timestamp
    sheet.setColumnWidth(2, 120);  // Fridge ID
    sheet.setColumnWidth(3, 90);   // Temp F
    sheet.setColumnWidth(4, 90);   // Temp C
    sheet.setColumnWidth(5, 110);  // Status
    sheet.setColumnWidth(6, 90);   // Firmware
    sheet.setColumnWidth(7, 200);  // Acknowledged By / Action

    Logger.log("Created new monthly sheet: " + tabName);
  }

  return sheet;
}

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

    var ss    = SpreadsheetApp.openById(SPREADSHEET_ID);
    var sheet = getMonthlySheet(ss);

    // ── Append reading row ──────────────────────────────────────────────────
    // Add new row to the top of the sheet
    sheet.insertRowAfter(1);
    var newRow = sheet.getRange(2, 1, 1, 6);
    newRow.setValues([[ts, fridge, tempF, tempC, status, version]]);

    // Clear formatting inherited from header row
    newRow.setBackground(null);
    newRow.setFontColor(null);
    newRow.setFontWeight("normal");

    // Force timestamp column to show date AND time
    sheet.getRange(2, 1).setNumberFormat("M/dd/yyyy HH:mm:ss");

    // ── Colour-code the status cell ─────────────────────────────────────────
    var statusCell = sheet.getRange(2, 5);
    if (status === "OK") {
      statusCell.setBackground("#d9ead3");          // light green
    } else if (status.startsWith("ALERT")) {
      statusCell.setBackground("#f4cccc");          // light red
    } else {
      statusCell.setBackground("#fff2cc");          // light yellow (SENSOR_ERROR etc)
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
  var props       = PropertiesService.getScriptProperties();
  var cooldownKey = "last_alert_" + fridge;
  var lastAlert   = props.getProperty(cooldownKey);
  var now         = new Date().getTime();

  // Check cooldown — suppress if already alerted recently
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
            + "Please check the refrigerator and update the Acknowledged By column immediately.\n";
  }

  MailApp.sendEmail(ALERT_EMAIL, subject, body);
  Logger.log("Alert sent for " + fridge + ": " + status);

  // Record timestamp to enforce cooldown
  props.setProperty(cooldownKey, now.toString());
}

// ── Utility: manually clear alert cooldown for a fridge (run from editor) ────
function clearAlertCooldown() {
  var props = PropertiesService.getScriptProperties();
  props.deleteAllProperties();
  Logger.log("All alert cooldowns cleared.");
}
const pages = document.querySelectorAll(".page");
const navButtons = document.querySelectorAll(".nav-btn");
const pageTitle = document.getElementById("pageTitle");

let lastStatus = null;
let isAuthenticated = false;
let currentUser = null;
let serverLogsLoaded = false;

function byId(id) {
  return document.getElementById(id);
}

function setText(id, value) {
  const el = byId(id);
  if (el) el.textContent = value;
}

function toNumber(value) {
  const n = Number(value);
  return Number.isFinite(n) ? n : 0;
}

function percent(value, max) {
  const n = Math.max(0, Math.min(100, (toNumber(value) / max) * 100));
  return n.toFixed(0) + "%";
}

function yesNo(value) {
  return value ? "YES" : "NO";
}

function connectedText(value) {
  return value ? "Connected" : "Not connected";
}

function formatMaybe(value, decimals, unit, connected) {
  if (!connected) return "Not connected";
  const n = toNumber(value);
  const text = decimals === 1 ? n.toFixed(1) : String(n);
  return unit ? text + " " + unit : text;
}

function computeHeatIndexC(tempC, humidity) {
  const t = toNumber(tempC);
  const rh = toNumber(humidity);

  if (t < 27 || rh < 40) {
    return t;
  }

  const f = (t * 9 / 5) + 32;
  const hiF =
    -42.379 +
    2.04901523 * f +
    10.14333127 * rh -
    0.22475541 * f * rh -
    0.00683783 * f * f -
    0.05481717 * rh * rh +
    0.00122874 * f * f * rh +
    0.00085282 * f * rh * rh -
    0.00000199 * f * f * rh * rh;

  return (hiF - 32) * 5 / 9;
}

function getHeatIndex(data, temp, hum) {
  if (Number.isFinite(Number(data.heat_index_c))) {
    return toNumber(data.heat_index_c);
  }

  return computeHeatIndexC(temp, hum);
}

function setState(id, warning, safeText, warnText) {
  const el = byId(id);
  if (!el) return;

  el.textContent = warning ? warnText : safeText;
  el.className = warning ? "state warn" : "state safe";
}

function setBar(id, value, max, warning) {
  const el = byId(id);
  if (!el) return;

  el.style.width = percent(value, max);
  el.className = warning ? "meter-fill warning" : "meter-fill";
}

function setSensorDisplay(valueId, barId, stateId, connected, value, decimals, max, warning, safeText, warnText, offlineText) {
  if (!connected) {
    setText(valueId, "--");

    const stateEl = byId(stateId);
    if (stateEl) {
      stateEl.textContent = offlineText;
      stateEl.className = "state offline";
    }

    const barEl = byId(barId);
    if (barEl) {
      barEl.style.width = "0%";
      barEl.className = "meter-fill offline";
    }

    return;
  }

  if (decimals === "1") {
    setText(valueId, toNumber(value).toFixed(1));
  } else {
    setText(valueId, value);
  }

  setState(stateId, warning, safeText, warnText);
  setBar(barId, value, max, warning);
}

function showPage(pageId) {
  pages.forEach(page => {
    page.classList.toggle("active", page.id === pageId);
  });

  navButtons.forEach(btn => {
    btn.classList.toggle("active", btn.dataset.page === pageId);
  });

  const activeBtn = document.querySelector(`.nav-btn[data-page="${pageId}"]`);
  if (activeBtn) pageTitle.textContent = activeBtn.textContent.trim();

  closeMobileNav();
}

navButtons.forEach(btn => {
  btn.addEventListener("click", () => showPage(btn.dataset.page));
});

/* ---------- Mobile nav (hamburger) ---------- */
const sidebarEl = byId("sidebar");
const mobileMenuBtn = byId("mobileMenuBtn");
const mobileNavOverlay = byId("mobileNavOverlay");

function openMobileNav() {
  if (sidebarEl) sidebarEl.classList.add("open");
  if (mobileNavOverlay) mobileNavOverlay.classList.add("show");
  if (mobileMenuBtn) mobileMenuBtn.setAttribute("aria-expanded", "true");
  document.body.classList.add("nav-open");
}

function closeMobileNav() {
  if (sidebarEl) sidebarEl.classList.remove("open");
  if (mobileNavOverlay) mobileNavOverlay.classList.remove("show");
  if (mobileMenuBtn) mobileMenuBtn.setAttribute("aria-expanded", "false");
  document.body.classList.remove("nav-open");
}

function toggleMobileNav() {
  if (sidebarEl && sidebarEl.classList.contains("open")) {
    closeMobileNav();
  } else {
    openMobileNav();
  }
}

if (mobileMenuBtn) {
  mobileMenuBtn.addEventListener("click", toggleMobileNav);
}

const sidebarCloseBtn = byId("sidebarCloseBtn");
if (sidebarCloseBtn) {
  sidebarCloseBtn.addEventListener("click", closeMobileNav);
}

if (mobileNavOverlay) {
  mobileNavOverlay.addEventListener("click", closeMobileNav);
}

window.addEventListener("resize", () => {
  if (window.innerWidth > 768) closeMobileNav();
});

/* ---------- Theme toggle ---------- */
const THEME_KEY = "helmet-theme";
const themeToggleBtn = byId("themeToggle");
const themeSwitch = byId("themeSwitch");
const themeLabel = themeToggleBtn ? themeToggleBtn.querySelector("span") : null;

function applyTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  const isDark = theme === "dark";

  if (themeSwitch) themeSwitch.classList.toggle("on", isDark);
  if (themeToggleBtn) themeToggleBtn.setAttribute("aria-pressed", String(isDark));

  if (themeLabel) {
    themeLabel.innerHTML = isDark
      ? '<svg><use href="#icon-moon" /></svg> Dark mode'
      : '<svg><use href="#icon-sun" /></svg> Light mode';
  }
}

function getStoredTheme() {
  try {
    return localStorage.getItem(THEME_KEY);
  } catch (err) {
    return null;
  }
}

function storeTheme(theme) {
  try {
    localStorage.setItem(THEME_KEY, theme);
  } catch (err) {
    /* storage unavailable, theme just won't persist */
  }
}

const prefersDark = window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches;
const initialTheme = getStoredTheme() || (prefersDark ? "dark" : "light");
applyTheme(initialTheme);

if (themeToggleBtn) {
  themeToggleBtn.addEventListener("click", () => {
    const next = document.documentElement.getAttribute("data-theme") === "dark" ? "light" : "dark";
    applyTheme(next);
    storeTheme(next);
  });
}

/* ---------- Auth modal ---------- */
const authModal = byId("authModal");
const authForm = byId("authForm");
const authTitle = byId("authTitle");
const authSubtitle = byId("authSubtitle");
const authSubmitBtn = byId("authSubmitBtn");
const authSwitchText = byId("authSwitchText");
const authSwitchBtn = byId("authSwitchBtn");
const authNameField = byId("authNameField");
const authCloseBtn = byId("authCloseBtn");

let authMode = "login";

function setAuthMode(mode) {
  authMode = mode;
  const isLogin = mode === "login";

  authTitle.textContent = isLogin ? "Log In" : "Create Account";
  authSubtitle.textContent = isLogin
    ? "Enter your account details."
    : "Create an account for this helmet.";
  authSubmitBtn.textContent = isLogin ? "Log In" : "Register";
  authSwitchText.textContent = isLogin ? "Don't have an account?" : "Already have an account?";
  authSwitchBtn.textContent = isLogin ? "Register" : "Log In";
  authNameField.style.display = isLogin ? "none" : "flex";
}

function openAuthModal(mode) {
  setAuthMode(mode);
  authModal.classList.add("show");
  document.body.classList.add("modal-open");
  closeMobileNav();
}

function closeAuthModal() {
  if (!isAuthenticated) {
    return;
  }

  authModal.classList.remove("show");
  document.body.classList.remove("modal-open");
}

if (authCloseBtn) authCloseBtn.addEventListener("click", closeAuthModal);
if (authSwitchBtn) {
  authSwitchBtn.addEventListener("click", () => setAuthMode(authMode === "login" ? "register" : "login"));
}
if (authModal) {
  authModal.addEventListener("click", (e) => {
    if (e.target === authModal) closeAuthModal();
  });
}
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape") {
    closeAuthModal();
    closeMobileNav();
  }
});

function setAppLocked(locked) {
  document.body.classList.toggle("auth-locked", locked);

  if (locked) {
    authModal.classList.add("show");
    document.body.classList.add("modal-open");
  } else {
    authModal.classList.remove("show");
    document.body.classList.remove("modal-open");
  }
}

function showLoginRequired() {
  isAuthenticated = false;
  currentUser = null;
  setLoggedOutUi();
  setAppLocked(true);
  openAuthModal("login");
}

function setLoggedInUi(user) {
  currentUser = user || null;
  const label = currentUser
    ? (currentUser.name || currentUser.email || "Account")
    : "Account";

  if (loginBtnTop) {
    loginBtnTop.innerHTML = '<svg class="icon"><use href="#icon-user" /></svg> ' + label;
  }

  if (logoutBtnTop) {
    logoutBtnTop.hidden = false;
  }

  if (mobileLoginBtn) {
    mobileLoginBtn.setAttribute("aria-label", "Log out");
  }

  const usersNavBtn = byId("usersNavBtn");
  if (usersNavBtn) {
    usersNavBtn.hidden = !(currentUser && currentUser.is_admin === true);
  }
}

function setLoggedOutUi() {
  currentUser = null;

  if (loginBtnTop) {
    loginBtnTop.innerHTML = '<svg class="icon"><use href="#icon-user" /></svg> Log In';
  }

  if (logoutBtnTop) {
    logoutBtnTop.hidden = true;
  }

  if (mobileLoginBtn) {
    mobileLoginBtn.setAttribute("aria-label", "Log in");
  }

  const usersNavBtn = byId("usersNavBtn");
  if (usersNavBtn) {
    usersNavBtn.hidden = true;
  }
}

function readAuthError(data, fallback) {
  return data && data.error ? data.error : fallback;
}

function apiFetch(url, options) {
  const requestOptions = options || {};
  requestOptions.credentials = "same-origin";

  return fetch(url, requestOptions).then(response => {
    if (response.status === 401) {
      showLoginRequired();
      throw new Error("LOGIN_REQUIRED");
    }

    if (!response.ok) {
      return response.json()
        .catch(() => null)
        .then(data => {
          throw new Error(readAuthError(data, "Request failed."));
        });
    }

    return response;
  });
}

function authRequest(path, payload) {
  return fetch(path, {
    method: "POST",
    credentials: "same-origin",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  }).then(response => {
    if (!response.ok) {
      return response.json()
        .catch(() => null)
        .then(data => {
          throw new Error(readAuthError(data, "Login failed."));
        });
    }

    return response.json();
  });
}

async function checkSession() {
  try {
    const response = await fetch("/api/auth/me", {
      method: "GET",
      credentials: "same-origin",
      cache: "no-store"
    });

    if (!response.ok) {
      isAuthenticated = false;
      setLoggedOutUi();
      setAppLocked(true);
      openAuthModal("login");
      return;
    }

    const user = await response.json();
    isAuthenticated = true;
    setLoggedInUi(user);
    setAppLocked(false);
    loadStatus();
    loadPinConfig();
    loadOtaStatus();
    loadServerLogs();

    if (user.is_admin === true) {
      loadUsers();
    }
  } catch (err) {
    isAuthenticated = false;
    serverLogsLoaded = false;
    setLoggedOutUi();
    setAppLocked(true);
    openAuthModal("login");
  }
}

function logout() {
  fetch("/api/auth/logout", {
    method: "POST",
    credentials: "same-origin"
  }).finally(() => {
    isAuthenticated = false;
    serverLogsLoaded = false;
    setLoggedOutUi();
    showPage("dashboard");
    setAppLocked(true);
    openAuthModal("login");
  });
}

if (authForm) {
  authForm.addEventListener("submit", (e) => {
    e.preventDefault();

    const name = byId("authName") ? byId("authName").value.trim() : "";
    const email = byId("authEmail") ? byId("authEmail").value.trim() : "";
    const password = byId("authPassword") ? byId("authPassword").value : "";

    if (!email || !password) {
      alert("Email and password are required.");
      return;
    }

    authSubmitBtn.disabled = true;
    authSubmitBtn.textContent = authMode === "login" ? "Logging in..." : "Creating...";

    const loginPayload = { email, password };
    const work = authMode === "register"
      ? authRequest("/api/auth/register", { name, email, password }).then(() => authRequest("/api/auth/login", loginPayload))
      : authRequest("/api/auth/login", loginPayload);

    work
      .then(() => {
        return checkSession();
      })
      .catch(err => {
        alert(err.message || "Login failed.");
      })
      .finally(() => {
        authSubmitBtn.disabled = false;
        setAuthMode(authMode);
      });
  });
}

function confirmLogout() {
  if (confirm("Log out now?")) {
    logout();
  }
}

function handleAccountButton() {
  if (isAuthenticated) {
    confirmLogout();
  } else {
    openAuthModal("login");
  }
}

const loginBtnTop = byId("loginBtnTop");
if (loginBtnTop) loginBtnTop.addEventListener("click", handleAccountButton);

const logoutBtnTop = byId("logoutBtnTop");
if (logoutBtnTop) logoutBtnTop.addEventListener("click", confirmLogout);

const mobileLoginBtn = byId("mobileLoginBtn");
if (mobileLoginBtn) mobileLoginBtn.addEventListener("click", handleAccountButton);

/* ---------- Reading history log + shift notes ---------- */
const MAX_LOG_ROWS = 500;
const NOTES_KEY = "helmet-shift-notes";
const sessionLog = [];

const logTableBody = byId("logTableBody");
const shiftNotes = byId("shiftNotes");

function formatClock(date) {
  return date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
}

function formatEntryTime(entry) {
  if (entry.timeLabel) return entry.timeLabel;
  if (entry.time instanceof Date) return formatClock(entry.time);
  return "--";
}

function logReading(data) {
  if (!logTableBody) return;

  const dhtConnected = data.dht_connected === true;
  const temp = toNumber(data.temperature_c);
  const hum = toNumber(data.humidity_percent);
  const heatIndex = getHeatIndex(data, temp, hum);

  const entry = {
    time: new Date(),
    gas: toNumber(data.gas_raw),
    flame: toNumber(data.flame_raw),
    temp,
    hum,
    heatIndex,
    dist: toNumber(data.distance_cm),
    buzzer: !!data.alarm_active,
    update_count: data.update_count
  };

  sessionLog.unshift(entry);
  if (sessionLog.length > MAX_LOG_ROWS) sessionLog.length = MAX_LOG_ROWS;

  renderLogTable();
}

function renderLogTable() {
  if (!logTableBody) return;

  if (sessionLog.length === 0) {
    logTableBody.innerHTML = `<tr id="logEmptyRow"><td colspan="8" class="muted">No readings logged yet this session.</td></tr>`;
    return;
  }

  logTableBody.innerHTML = sessionLog
    .map(entry => `
      <tr${entry.buzzer ? ' class="log-row-alarm"' : ""}>
        <td>${formatEntryTime(entry)}</td>
        <td>${entry.gas}</td>
        <td>${entry.flame}</td>
        <td>${entry.temp.toFixed(1)}</td>
        <td>${entry.hum.toFixed(1)}</td>
        <td>${entry.heatIndex.toFixed(1)}</td>
        <td>${entry.dist.toFixed(1)}</td>
        <td>${entry.buzzer ? "ON" : "OFF"}</td>
      </tr>
    `)
    .join("");
}


function logEntryFromServer(item) {
  const temp = toNumber(item.temperature_c);
  const hum = toNumber(item.humidity_percent);
  const heatIndex = getHeatIndex(item, temp, hum);
  const alarm = !!(item.gas_warning || item.flame_warning || item.heat_index_warning || item.distance_warning);

  return {
    timeLabel: "#" + (item.update_count || "--"),
    update_count: item.update_count || 0,
    gas: toNumber(item.gas_raw),
    flame: toNumber(item.flame_raw),
    temp,
    hum,
    heatIndex,
    dist: toNumber(item.distance_cm),
    buzzer: alarm
  };
}

function loadServerLogs() {
  if (!isAuthenticated || serverLogsLoaded) return;

  apiFetch("/api/logs", { cache: "no-store" })
    .then(response => response.json())
    .then(data => {
      serverLogsLoaded = true;

      if (!data.logs || !Array.isArray(data.logs)) return;

      sessionLog.length = 0;

      data.logs
        .slice()
        .reverse()
        .forEach(item => sessionLog.push(logEntryFromServer(item)));

      if (sessionLog.length > MAX_LOG_ROWS) {
        sessionLog.length = MAX_LOG_ROWS;
      }

      renderLogTable();
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        serverLogsLoaded = true;
      }
    });
}

function exportLogCsv() {
  if (sessionLog.length === 0) {
    alert("No readings to export yet.");
    return;
  }

  const header = "time,gas_raw,flame_raw,temperature_c,humidity_percent,heat_index_c,distance_cm,buzzer_alarm";
  const rows = sessionLog
    .slice()
    .reverse()
    .map(entry => [
      entry.time instanceof Date ? entry.time.toISOString() : (entry.timeLabel || ""),
      entry.gas,
      entry.flame,
      entry.temp.toFixed(1),
      entry.hum.toFixed(1),
      entry.heatIndex.toFixed(1),
      entry.dist.toFixed(1),
      entry.buzzer ? "1" : "0"
    ].join(","));

  const csv = [header, ...rows].join("\n");
  const blob = new Blob([csv], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);

  const link = document.createElement("a");
  link.href = url;
  link.download = `helmet-log-${Date.now()}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

function clearLog() {
  if (sessionLog.length === 0) return;
  if (!confirm("Clear all logged readings?")) return;

  sessionLog.length = 0;
  renderLogTable();

  if (currentUser && currentUser.is_admin === true) {
    apiFetch("/api/logs/clear", { method: "POST" })
      .catch(err => {
        if (err.message !== "LOGIN_REQUIRED") {
          alert(err.message || "Local table cleared, but device log was not cleared.");
        }
      });
  }
}

const exportLogBtn = byId("exportLogBtn");
const clearLogBtn = byId("clearLogBtn");
if (exportLogBtn) exportLogBtn.addEventListener("click", exportLogCsv);
if (clearLogBtn) clearLogBtn.addEventListener("click", clearLog);


/* ---------- Admin users ---------- */
const usersTableBody = byId("usersTableBody");
const reloadUsersBtn = byId("reloadUsersBtn");

function renderUsers(users) {
  if (!usersTableBody) return;

  if (!users || users.length === 0) {
    usersTableBody.innerHTML = '<tr><td colspan="3" class="muted">No users found.</td></tr>';
    return;
  }

  usersTableBody.innerHTML = users.map(user => `
    <tr>
      <td>${user.name || "--"}</td>
      <td>${user.email || "--"}</td>
      <td>${user.role || "user"}</td>
    </tr>
  `).join("");
}

function loadUsers() {
  if (!isAuthenticated || !currentUser || currentUser.is_admin !== true) return;

  apiFetch("/api/auth/users", { cache: "no-store" })
    .then(response => response.json())
    .then(data => renderUsers(data.users || []))
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED" && usersTableBody) {
        usersTableBody.innerHTML = '<tr><td colspan="3" class="muted">Unable to load users.</td></tr>';
      }
    });
}

if (reloadUsersBtn) {
  reloadUsersBtn.addEventListener("click", loadUsers);
}

if (shiftNotes) {
  try {
    const savedNotes = localStorage.getItem(NOTES_KEY);
    if (savedNotes) shiftNotes.value = savedNotes;
  } catch (err) {
    /* storage unavailable, notes just won't persist */
  }

  shiftNotes.addEventListener("input", () => {
    try {
      localStorage.setItem(NOTES_KEY, shiftNotes.value);
    } catch (err) {
      /* storage unavailable, notes just won't persist */
    }
  });
}

/* ---------- Dashboard ---------- */
const GAUGE_CIRCUMFERENCE = 251.3;

function setOutputState(id, active, activeText, inactiveText) {
  const el = byId(id);
  if (!el) return;

  el.textContent = active ? activeText : inactiveText;
  el.className = active ? "state warn" : "state safe";
}

function updateOutputIndicators(data) {
  const outputs = data.outputs || {};

  setOutputState("ledGasState", outputs.led_gas === true || data.gas_warning === true, "ON", "OFF");
  setOutputState("ledFlameState", outputs.led_flame === true || data.flame_warning === true, "ON", "OFF");
  setOutputState("ledHeatIndexState", outputs.led_heat_index === true || data.heat_index_warning === true, "ON", "OFF");
  setOutputState("ledDistanceState", outputs.led_distance === true || data.distance_warning === true, "ON", "OFF");
  setOutputState("buzzerState", outputs.buzzer === true || data.alarm_active === true, "SOUNDING", "OFF");
}

function updateDashboard(data) {
  lastStatus = data;
  logReading(data);

  const gasConnected = data.gas_connected === true;
  const flameConnected = data.flame_connected === true;
  const dhtConnected = data.dht_connected === true;
  const ultrasonicConnected = data.ultrasonic_connected === true;

  const gas = toNumber(data.gas_raw);
  const flame = toNumber(data.flame_raw);
  const temp = toNumber(data.temperature_c);
  const hum = toNumber(data.humidity_percent);
  const heatIndex = getHeatIndex(data, temp, hum);
  const dist = toNumber(data.distance_cm);

  const heatIndexWarning = data.heat_index_warning === true ||
    data.temperature_warning === true ||
    data.humidity_warning === true;

  setSensorDisplay("gasValue", "gasBar", "gasState", gasConnected, gas, "", 1023, data.gas_warning, "Gas level normal", "Gas warning detected", "Gas sensor not connected");
  setSensorDisplay("flameValue", "flameBar", "flameState", flameConnected, flame, "", 1023, data.flame_warning, "No flame detected", "Flame warning detected", "Flame sensor not connected");
  setSensorDisplay("tempValue", "tempBar", "tempState", dhtConnected, temp, "1", 60, false, "DHT temperature reading", "DHT warning", "DHT sensor not connected");
  setSensorDisplay("humValue", "humBar", "humState", dhtConnected, hum, "1", 100, false, "DHT humidity reading", "DHT warning", "DHT sensor not connected");
  setSensorDisplay("heatIndexValue", "heatIndexBar", "heatIndexState", dhtConnected, heatIndex, "1", 80, heatIndexWarning, "Computed heat index normal", "Computed heat index warning", "DHT sensor not connected");
  setSensorDisplay("distValue", "distBar", "distState", ultrasonicConnected, dist, "1", 200, data.distance_warning, "Distance normal", "Obstacle too close", "Ultrasonic sensor not connected");

  updateOutputIndicators(Object.assign({}, data, { heat_index_warning: heatIndexWarning }));

  setText("wifiStatus", data.wifi_connected ? "Connected" : "Disconnected");
  setText("alarmStatus", data.alarm_active ? "Buzzer ON" : "Buzzer OFF");
  setText("updateCount", data.update_count);
  setText("apiStatus", "Online");

  setText("tableGas", formatMaybe(gas, 0, "raw", gasConnected));
  setText("tableFlame", formatMaybe(flame, 0, "raw", flameConnected));
  setText("tableTemp", formatMaybe(temp, 1, "°C", dhtConnected));
  setText("tableHum", formatMaybe(hum, 1, "%", dhtConnected));
  setText("tableHeatIndex", formatMaybe(heatIndex, 1, "°C", dhtConnected));
  setText("tableDist", formatMaybe(dist, 1, "cm", ultrasonicConnected));
  setText("tableGasConn", connectedText(gasConnected));
  setText("tableFlameConn", connectedText(flameConnected));
  setText("tableDhtConn", connectedText(dhtConnected));
  setText("tableUltraConn", connectedText(ultrasonicConnected));
  setText("tableGasWarn", yesNo(data.gas_warning));
  setText("tableFlameWarn", yesNo(data.flame_warning));
  setText("tableHeatIndexWarn", yesNo(heatIndexWarning));
  setText("tableDistWarn", yesNo(data.distance_warning));
  setText("tableAlarm", data.alarm_active ? "BUZZER SOUNDING" : "OFF");
  setText("tableAlive", yesNo(data.system_alive));

  const badge = byId("systemBadge");
  const sideDot = byId("sideStatusDot");
  const sideText = byId("sideStatusText");
  const dangerPanel = byId("dangerPanel");
  const dangerTitle = byId("dangerTitle");
  const dangerText = byId("dangerText");
  const gaugeFill = byId("gaugeFill");
  const gaugeIcon = byId("gaugeIcon");

  if (data.system_alive) {
    badge.textContent = "System Online";
    badge.className = "badge safe";
    sideDot.className = "dot online";
    sideText.textContent = "System online";
  } else {
    badge.textContent = "System Offline";
    badge.className = "badge warning";
    sideDot.className = "dot offline";
    sideText.textContent = "System offline";
  }

  if (data.alarm_active) {
    const activeWarnings = [];
    if (data.gas_warning) activeWarnings.push("gas");
    if (data.flame_warning) activeWarnings.push("flame");
    if (heatIndexWarning) activeWarnings.push("heat index");
    if (data.distance_warning) activeWarnings.push("distance");

    dangerPanel.className = "hero-card danger";
    dangerTitle.textContent = "Danger Detected";
    dangerText.textContent = "Buzzer alarm is active because of: " + (activeWarnings.join(", ") || "unknown warning") + ".";
    gaugeFill.setAttribute("stroke-dashoffset", "0");
    gaugeFill.classList.add("danger");
    gaugeIcon.classList.add("danger");
    gaugeIcon.innerHTML = '<svg><use href="#icon-alert" /></svg>';
  } else {
    dangerPanel.className = "hero-card";
    dangerTitle.textContent = "Helmet Safe";
    dangerText.textContent = "Gas, flame, heat index, and distance readings are inside their safe range.";
    gaugeFill.setAttribute("stroke-dashoffset", "0");
    gaugeFill.classList.remove("danger");
    gaugeIcon.classList.remove("danger");
    gaugeIcon.innerHTML = '<svg><use href="#icon-shield-check" /></svg>';
  }
}

function showApiLost() {
  setText("apiStatus", "Offline");

  const badge = byId("systemBadge");
  const sideDot = byId("sideStatusDot");
  const sideText = byId("sideStatusText");
  const gaugeFill = byId("gaugeFill");
  const gaugeIcon = byId("gaugeIcon");

  badge.textContent = "Connection Lost";
  badge.className = "badge warning";
  sideDot.className = "dot offline";
  sideText.textContent = "API offline";

  setText("dangerTitle", "Connection Lost");
  setText("dangerText", "The browser can't reach the ESP8266 at /api/status. Check that the helmet is powered and on the same network.");

  if (gaugeFill) {
    gaugeFill.setAttribute("stroke-dashoffset", String(GAUGE_CIRCUMFERENCE));
    gaugeFill.classList.remove("danger");
  }
  if (gaugeIcon) {
    gaugeIcon.classList.remove("danger");
    gaugeIcon.innerHTML = '<svg><use href="#icon-wifi" /></svg>';
  }
}

function loadStatus() {
  if (!isAuthenticated) {
    return;
  }

  apiFetch("/api/status", { cache: "no-store" })
    .then(response => response.json())
    .then(data => updateDashboard(data))
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        showApiLost();
      }
    });
}

function renderWifiList(networks) {
  const list = byId("wifiList");
  list.innerHTML = "";

  if (!networks || networks.length === 0) {
    list.innerHTML = `<div class="notice"><svg class="icon"><use href="#icon-info" /></svg><span>No networks found.</span></div>`;
    return;
  }

  networks.forEach(net => {
    const item = document.createElement("div");
    item.className = "wifi-item";

    const ssid = net.ssid || "Hidden network";
    const rssi = net.rssi || 0;
    const auth = net.auth || "unknown";

    item.innerHTML = `
      <div class="wifi-item-name">
        <svg class="icon"><use href="#icon-wifi" /></svg>
        <div>
          <strong>${ssid}</strong>
          <span>RSSI: ${rssi} dBm • Security: ${auth}</span>
        </div>
      </div>
      <button class="small-btn">Select</button>
    `;

    item.querySelector("button").addEventListener("click", () => {
      byId("wifiSsid").value = ssid;
    });

    list.appendChild(item);
  });
}

function scanWifi() {
  const list = byId("wifiList");
  list.innerHTML = `<div class="notice"><svg class="icon"><use href="#icon-info" /></svg><span>Scanning networks...</span></div>`;

  apiFetch("/api/wifi/scan", { cache: "no-store" })
    .then(response => {
      return response.json();
    })
    .then(data => renderWifiList(data.networks || []))
    .catch(err => {
      if (err.message === "LOGIN_REQUIRED") {
        list.innerHTML = `
          <div class="notice">
            <svg class="icon"><use href="#icon-info" /></svg>
            <span>Log in first, then scan nearby Wi-Fi networks.</span>
          </div>
        `;
        return;
      }

      list.innerHTML = `
        <div class="notice">
          <svg class="icon"><use href="#icon-info" /></svg>
          <span>${err.message || "Wi-Fi scan endpoint isn't ready yet."}</span>
        </div>
      `;
    });
}

function connectWifi(event) {
  event.preventDefault();

  const ssid = byId("wifiSsid").value.trim();
  const password = byId("wifiPassword").value;

  if (!ssid) {
    alert("Enter a Wi-Fi SSID first.");
    return;
  }

  apiFetch("/api/wifi/connect", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ ssid, password })
  })
    .then(response => {
      alert("Wi-Fi connection request sent.");
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        alert(err.message || "Firmware endpoint POST /api/wifi/connect isn't ready yet.");
      }
    });
}

function saveThresholds(event) {
  event.preventDefault();

  const config = {
    gas_warning_level: Number(byId("gasLimit").value),
    flame_warning_level: Number(byId("flameLimit").value),
    heat_index_warning_level: Number(byId("heatIndexLimit").value),
    distance_warning_cm: Number(byId("distLimit").value)
  };

  apiFetch("/api/config/thresholds", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(config)
  })
    .then(() => {
      alert("Thresholds saved.");
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        alert(err.message || "Firmware endpoint POST /api/config/thresholds is not ready yet.");
      }
    });
}

/* ---------- Hardware pin configuration ---------- */
const ADC_OPTIONS = ["A0"];

const DIGITAL_PIN_OPTIONS = [
  "D0",
  "D1",
  "D2",
  "D3",
  "D4",
  "D5",
  "D6",
  "D7",
  "D8"
];

const PIN_OPTIONS_BY_SELECT = {
  gas_digitalPin: DIGITAL_PIN_OPTIONS,
  flame_adcPin: ADC_OPTIONS,
  dht_dataPin: DIGITAL_PIN_OPTIONS,
  ultrasonic_trigPin: DIGITAL_PIN_OPTIONS,
  ultrasonic_echoPin: DIGITAL_PIN_OPTIONS,
  led_gasPin: DIGITAL_PIN_OPTIONS,
  led_flamePin: DIGITAL_PIN_OPTIONS,
  led_heat_indexPin: DIGITAL_PIN_OPTIONS,
  led_distancePin: DIGITAL_PIN_OPTIONS,
  buzzerPin: DIGITAL_PIN_OPTIONS
};

const DIGITAL_PIN_SELECT_IDS = [
  "gas_digitalPin",
  "dht_dataPin",
  "ultrasonic_trigPin",
  "ultrasonic_echoPin",
  "led_gasPin",
  "led_flamePin",
  "led_heat_indexPin",
  "led_distancePin",
  "buzzerPin"
];

const PIN_SELECT_IDS = [
  "gas_digitalPin",
  "flame_adcPin",
  ...DIGITAL_PIN_SELECT_IDS.filter(id => id !== "gas_digitalPin")
];

const DEFAULT_PIN_VALUES = {
  gas_digitalPin: "D6",
  flame_adcPin: "A0",
  dht_dataPin: "D1",
  ultrasonic_trigPin: "D2",
  ultrasonic_echoPin: "D5",
  led_gasPin: "D0",
  led_flamePin: "D3",
  led_heat_indexPin: "D4",
  led_distancePin: "D8",
  buzzerPin: "D7"
};

let pinSelectsInitialized = false;

function optionIsAllowedForSelect(id, value) {
  const allowed = PIN_OPTIONS_BY_SELECT[id] || [];
  return allowed.includes(value);
}

function getUsedDigitalPins(exceptId) {
  const used = new Set();

  DIGITAL_PIN_SELECT_IDS.forEach(id => {
    if (id === exceptId) return;

    const select = byId(id);

    if (select && select.value) {
      used.add(select.value);
    }
  });

  return used;
}

function renderPinSelect(id) {
  const select = byId(id);
  if (!select) return;

  const current = select.value || DEFAULT_PIN_VALUES[id] || "";
  const options = PIN_OPTIONS_BY_SELECT[id] || [];
  const used = DIGITAL_PIN_SELECT_IDS.includes(id) ? getUsedDigitalPins(id) : new Set();

  select.innerHTML = "";

  options.forEach(value => {
    if (used.has(value) && value !== current) {
      return;
    }

    const option = document.createElement("option");
    option.value = value;
    option.textContent = value;
    select.appendChild(option);
  });

  if (current && optionIsAllowedForSelect(id, current)) {
    const exists = Array.from(select.options).some(option => option.value === current);

    if (!exists) {
      const option = document.createElement("option");
      option.value = current;
      option.textContent = current;
      select.appendChild(option);
    }

    select.value = current;
  }

  if (!select.value && select.options.length > 0) {
    select.value = select.options[0].value;
  }
}

function updatePinDropdowns() {
  PIN_SELECT_IDS.forEach(renderPinSelect);
}

function initPinSelects() {
  updatePinDropdowns();

  if (pinSelectsInitialized) {
    return;
  }

  PIN_SELECT_IDS.forEach(id => {
    const select = byId(id);

    if (select) {
      select.addEventListener("change", updatePinDropdowns);
    }
  });

  pinSelectsInitialized = true;
}

function setPinValue(id, value) {
  const select = byId(id);
  if (!select || value === undefined || value === null) return;

  const text = String(value);

  if (!optionIsAllowedForSelect(id, text)) {
    return;
  }

  const exists = Array.from(select.options).some(option => option.value === text);

  if (!exists) {
    const option = document.createElement("option");
    option.value = text;
    option.textContent = text;
    select.appendChild(option);
  }

  select.value = text;
}

function validateUniqueDigitalPins() {
  const used = new Map();

  for (const id of DIGITAL_PIN_SELECT_IDS) {
    const select = byId(id);
    if (!select) continue;

    if (used.has(select.value)) {
      alert("Pin " + select.value + " is already used by another function. Choose a different pin.");
      return false;
    }

    used.set(select.value, id);
  }

  return true;
}

function loadPinConfig() {
  initPinSelects();

  if (!isAuthenticated || !byId("pinConfigForm")) {
    return;
  }

  apiFetch("/api/config/pins", { cache: "no-store" })
    .then(response => response.json())
    .then(data => {
      const pins = data.pins || data;

      setPinValue("gas_digitalPin", pins.gas_digital || pins.gas_digital_gpio || "D6");
      setPinValue("flame_adcPin", pins.flame_adc || pins.flame_adc_channel || "A0");
      setPinValue("dht_dataPin", pins.dht_data || pins.dht_gpio || "D1");
      setPinValue("ultrasonic_trigPin", pins.ultrasonic_trig || pins.ultrasonic_trig_gpio || "D2");
      setPinValue("ultrasonic_echoPin", pins.ultrasonic_echo || pins.ultrasonic_echo_gpio || "D5");
      setPinValue("led_gasPin", pins.led_gas || pins.led_gas_gpio || "D0");
      setPinValue("led_flamePin", pins.led_flame || pins.led_flame_gpio || "D3");
      setPinValue("led_heat_indexPin", pins.led_heat_index || pins.led_heat_index_gpio || "D4");
      setPinValue("led_distancePin", pins.led_distance || pins.led_distance_gpio || "D8");
      setPinValue("buzzerPin", pins.buzzer || pins.buzzer_gpio || "D7");

      updatePinDropdowns();
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        updatePinDropdowns();
      }
    });
}

function savePinConfig(event) {
  event.preventDefault();

  if (!validateUniqueDigitalPins()) {
    return;
  }

  const config = {
    gas_digital: byId("gas_digitalPin").value,
    flame_adc: byId("flame_adcPin").value,
    dht_data: byId("dht_dataPin").value,
    ultrasonic_trig: byId("ultrasonic_trigPin").value,
    ultrasonic_echo: byId("ultrasonic_echoPin").value,
    led_gas: byId("led_gasPin").value,
    led_flame: byId("led_flamePin").value,
    led_heat_index: byId("led_heat_indexPin").value,
    led_distance: byId("led_distancePin").value,
    buzzer: byId("buzzerPin").value
  };

  apiFetch("/api/config/pins", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(config)
  })
    .then(() => {
      alert("Pin configuration saved and applied.");
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        alert(err.message || "Firmware endpoint POST /api/config/pins is not ready yet.");
      }
    });
}



/* ---------- OTA updates ---------- */
function formatBytes(value) {
  const bytes = Number(value || 0);

  if (!bytes) return "--";
  if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB";
  if (bytes >= 1024) return (bytes / 1024).toFixed(1) + " KB";
  return bytes + " B";
}

function loadOtaStatus() {
  if (!isAuthenticated || !byId("otaRunningPartition")) {
    return;
  }

  apiFetch("/api/ota/status", { cache: "no-store" })
    .then(response => response.json())
    .then(data => {
      setText("otaRunningPartition", data.running_partition || "--");
      setText("otaNextPartition", data.next_update_partition || "--");
      setText("otaSlotSize", formatBytes(data.next_update_size));
      setText("otaSpiffsSize", formatBytes(data.storage_size));
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        setText("otaRunningPartition", "Endpoint missing");
        setText("otaNextPartition", "--");
        setText("otaSlotSize", "--");
        setText("otaSpiffsSize", "--");
      }
    });
}

function setUploadProgress(progressId, percentValue) {
  const bar = byId(progressId);
  if (!bar) return;
  bar.style.width = Math.max(0, Math.min(100, percentValue)) + "%";
}

function setUploadStatus(statusId, text, isError) {
  const el = byId(statusId);
  if (!el) return;
  el.textContent = text;
  el.className = isError ? "upload-status state warn" : "upload-status muted";
}

function uploadBinaryFile(options) {
  const fileInput = byId(options.fileInputId);
  const button = byId(options.buttonId);
  const file = fileInput && fileInput.files && fileInput.files[0];

  if (!file) {
    setUploadStatus(options.statusId, "Choose a .bin file first.", true);
    return;
  }

  if (!file.name.toLowerCase().endsWith(".bin")) {
    setUploadStatus(options.statusId, "Use a .bin file only.", true);
    return;
  }

  if (!confirm(options.confirmText + "\n\nFile: " + file.name + "\nSize: " + formatBytes(file.size))) {
    return;
  }

  const xhr = new XMLHttpRequest();
  xhr.open("POST", options.endpoint, true);
  xhr.withCredentials = true;
  xhr.setRequestHeader("Content-Type", "application/octet-stream");

  setUploadProgress(options.progressId, 0);
  setUploadStatus(options.statusId, "Uploading " + file.name + "...", false);
  if (button) button.disabled = true;

  xhr.upload.onprogress = function (event) {
    if (event.lengthComputable) {
      setUploadProgress(options.progressId, (event.loaded / event.total) * 100);
    }
  };

  xhr.onload = function () {
    if (button) button.disabled = false;

    let data = null;
    try {
      data = JSON.parse(xhr.responseText || "{}");
    } catch (err) {
      data = null;
    }

    if (xhr.status === 401) {
      showLoginRequired();
      setUploadStatus(options.statusId, "Login required.", true);
      return;
    }

    if (xhr.status < 200 || xhr.status >= 300) {
      const message = data && data.error ? data.error : "Upload failed.";
      setUploadStatus(options.statusId, message, true);
      return;
    }

    setUploadProgress(options.progressId, 100);
    setUploadStatus(options.statusId, (data && data.message) ? data.message : "Upload complete. Device is rebooting.", false);

    setTimeout(() => {
      loadOtaStatus();
    }, 4000);
  };

  xhr.onerror = function () {
    if (button) button.disabled = false;
    setUploadStatus(options.statusId, "Network error during upload.", true);
  };

  xhr.send(file);
}

function postAction(url, missingMessage) {
  apiFetch(url, { method: "POST" })
    .then(response => {
      alert("Command sent.");
    })
    .catch(err => {
      if (err.message !== "LOGIN_REQUIRED") {
        alert(err.message || missingMessage);
      }
    });
}

byId("scanWifiBtn").addEventListener("click", scanWifi);
byId("wifiForm").addEventListener("submit", connectWifi);
byId("thresholdForm").addEventListener("submit", saveThresholds);

const pinConfigForm = byId("pinConfigForm");
if (pinConfigForm) pinConfigForm.addEventListener("submit", savePinConfig);
initPinSelects();

byId("testAlarmBtn").addEventListener("click", () => {
  postAction("/api/alarm/test", "Alarm test failed.");
});

byId("restartBtn").addEventListener("click", () => {
  if (confirm("Restart ESP8266 device?")) {
    postAction("/api/system/restart", "Restart failed.");
  }
});

const refreshOtaStatusBtn = byId("refreshOtaStatusBtn");
if (refreshOtaStatusBtn) refreshOtaStatusBtn.addEventListener("click", loadOtaStatus);

const firmwareOtaBtn = byId("firmwareOtaBtn");
if (firmwareOtaBtn) {
  firmwareOtaBtn.addEventListener("click", () => {
    uploadBinaryFile({
      endpoint: "/api/ota/firmware",
      fileInputId: "firmwareOtaFile",
      progressId: "firmwareOtaProgress",
      statusId: "firmwareOtaStatus",
      buttonId: "firmwareOtaBtn",
      confirmText: "Upload firmware OTA? The device will reboot after success."
    });
  });
}

const spiffsOtaBtn = byId("spiffsOtaBtn");
if (spiffsOtaBtn) {
  spiffsOtaBtn.addEventListener("click", () => {
    uploadBinaryFile({
      endpoint: "/api/ota/spiffs",
      fileInputId: "spiffsOtaFile",
      progressId: "spiffsOtaProgress",
      statusId: "spiffsOtaStatus",
      buttonId: "spiffsOtaBtn",
      confirmText: "Upload SPIFFS/UI OTA? The web files will be replaced and the device will reboot after success."
    });
  });
}

checkSession();
setInterval(() => {
  if (isAuthenticated) {
    loadStatus();
  }
}, 1000);






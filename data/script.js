// === GLOBAL VARIABLES ===
let ws;
let reconnecting = false;
let reconnectAttempts = 0;
let heartbeatInterval;
let lastPongReceived = true;
let pendingRequests = {};
let requestIdCounter = 0;
let upgradeAvailable = false;
let isRequestInProgress = false;
let acknowledgmentReceived = false;
let currentFW = "1.0.1";

let moduleName = "Smart Controller";
let websiteName = "webpage";
let customMQTTAddress = "1.1.1.1";
let customMQTTPort = "1883";
let smartboat = false;
let customMqtt = false;

 // Define globally accessible functions
 function showLoadingIndicator() {
    document.getElementById('loadingIndicator').style.display = 'block';
}

function hideLoadingIndicator() {
    document.getElementById('loadingIndicator').style.display = 'none';
}

function requestSettings() {
    const requestData = JSON.stringify({ type: 'requestSettings' });
    sendDataToServer(requestData, true); // Expecting a response
}

function sendDataToServer(data, expectResponse = false, timeoutDuration = 5000) {
    return new Promise((resolve, reject) => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            console.warn("❌ WebSocket is not open. Attempting to recover...");
            tryReconnectWebSocket();  // optional recovery logic
            return reject(new Error('WebSocket is not open.'));
        }

        const requestId = generateRequestId();
        data.requestId = requestId;

        if (expectResponse) {
            pendingRequests[requestId] = {
                resolve,
                timeout: setTimeout(() => {
                    if (pendingRequests[requestId]) {
                        delete pendingRequests[requestId];
                        console.error('⏰ Timeout for requestId:', requestId);
                        reject(new Error('Request timed out'));
                    }
                }, timeoutDuration)
            };
        }

        console.log("📤 Sending to server:", JSON.stringify(data, null, 2));
        ws.send(JSON.stringify(data));
    });
}

function tryReconnectWebSocket() {
    if (reconnectInProgress) return;

    reconnectInProgress = true;
    console.log("🔁 Attempting WebSocket reconnection...");

    setTimeout(() => {
        setupWebSocket();  // Your existing reconnect logic
        reconnectInProgress = false;
    }, 1000);  // Could be exponential backoff, etc.
}

function enforceCharacterLimit(event) {
    const maxLength = 16;
    const inputField = event.target;

    if (inputField.value.length > maxLength) {
        alert("⚠️ Maximum character limit (16) reached!");
        inputField.value = inputField.value.substring(0, maxLength); // Trim excess
    }
}

function setupWebSocket() {
    // 🔄 Clear previous handlers if they exist
    if (ws) {
        console.warn("♻️ Cleaning up old WebSocket handlers...");
        ws.onopen = null;
        ws.onclose = null;
        ws.onerror = null;
        ws.onmessage = null;
        ws = null;
    }

    if (reconnecting) {
        console.warn("⚠️ Reconnect already in progress. Skipping setup.");
        return;
    }

    console.log("🔌 Establishing WebSocket connection to ESP32...");
    ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onopen = function () {
        console.log("✅ WebSocket connection opened");
        hideWebSocketBanner();

        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ data: "getData" }));
        } else {
            console.warn("⚠️ WebSocket not fully open during onopen.");
        }

        reconnecting = false;
        reconnectAttempts = 0;
        startHeartbeat();
    };

    ws.onmessage = function (event) {
        console.log("📩 Raw data received:", event.data);
        try {
            const parsedData = JSON.parse(event.data);
            if (parsedData.type === "pong") {
                console.log("🏓 Pong received! Server is alive.");
                lastPongReceived = true;
                return;
            }
            handleWebSocketMessage(parsedData);
        } catch (error) {
            console.error("❌ Failed to parse WebSocket message:", error);
        }
    };

    ws.onerror = function (error) {
        console.error("⚠️ WebSocket error:", error);
        showWebSocketBanner();
    };

    ws.onclose = function () {
        console.warn("❌ WebSocket closed. Will attempt to reconnect...");
        stopHeartbeat();
        showWebSocketBanner();

        if (!reconnecting) {
            reconnecting = true;
            const delay = Math.min(5000, 1000 * Math.pow(2, reconnectAttempts));
            reconnectAttempts++;

            setTimeout(() => {
                console.warn(`🔁 Attempting reconnect #${reconnectAttempts} after ${delay}ms`);
                setupWebSocket();
            }, delay);
        }
    };
}


function showWebSocketBanner() {
    const banner = document.getElementById("wsStatusBanner");
    if (banner) banner.style.display = "block";
}

function hideWebSocketBanner() {
    const banner = document.getElementById("wsStatusBanner");
    if (banner) banner.style.display = "none";
}

// **Heartbeat Functions**
function startHeartbeat() {
    stopHeartbeat(); // Ensure no duplicate intervals

    heartbeatInterval = setInterval(() => {
        if (!lastPongReceived) {
            console.warn("🚨 No Pong received! Closing WebSocket and reconnecting...");
            ws.close(); // Force reconnect
            return;
        }

        console.log("🏓 Sending Ping...");
        ws.send(JSON.stringify({ type: "ping" }));
        lastPongReceived = false; // Expect a pong response
    }, 30000); // Every 30 seconds
}

function stopHeartbeat() {
    if (heartbeatInterval) {
        clearInterval(heartbeatInterval);
        heartbeatInterval = null;
    }
}

function handleWebSocketMessage(data) {
    try {
        let parsedData = typeof data === 'string' ? JSON.parse(data) : data;

        console.log('Parsed response:', parsedData);

        // Check if this is a response to a specific request (using requestId)
        if (parsedData.requestId && pendingRequests[parsedData.requestId]) {
            let resolve = pendingRequests[parsedData.requestId].resolve;

            // Clear the timeout for the request
            clearTimeout(pendingRequests[parsedData.requestId].timeout);
            delete pendingRequests[parsedData.requestId];  // Prevent any future processing of this request

            resolve(parsedData);  // Resolve the promise with the response data
        } else if (parsedData.type === 'ack') {
            console.log('Server acknowledged the request successfully.');
            // Handle ack here, if necessary
        } else {
            // Handle unsolicited messages here
            processUnsolicitedMessage(parsedData);
        }

    } catch (error) {
        console.error('Failed to process WebSocket message:', error);
    }
}

function processUnsolicitedMessage(parsedData) {
    switch (parsedData.type) {
        case 'configData':
        console.log('Handling configData...');
        showLoadingIndicator(); // 👈 Start loader before processing
        setTimeout(() => {
            processConfigData(parsedData);
            hideLoadingIndicator(); // 👈 Hide once done
        }, 50); // Optional delay to let UI render spinner first
        break;

        case 'upgradeProgress':
                console.log('Handling upgradeProgress...');
                showUpgradeProgress(parsedData);
                break;
    
        case 'otaProgress':
            console.log('Handling otaProgress...');
            updateOtaProgress(parsedData);
            break;

        case 'ack':
            console.log('Server acknowledged the request successfully.');
            if (parsedData.requestId && pendingRequests[parsedData.requestId]) {
                let resolve = pendingRequests[parsedData.requestId].resolve;
                clearTimeout(pendingRequests[parsedData.requestId].timeout);
                delete pendingRequests[parsedData.requestId];
                resolve(parsedData);
            }
            break;
        
            default:
                console.warn('Unknown message type received:', parsedData.type);
                alert('Unknown message type received: ' + parsedData.type);
                break;
    }
}

function generateRequestId() {
    requestIdCounter += 1;
    return `req-${requestIdCounter}-${Date.now()}`;  // Unique ID based on a counter and timestamp
}

function processConfigData(data) {
    console.log("📡 Processing configuration data from server:", data);

    // ✅ Module info
    moduleName = data.modName || "Smart Module";
    websiteName = data.webname || "Web UI";
    smartboat = data.smartboat || false;
    customMqtt = data.customMqtt || false;
    currentFW = data.firmwareVersion || "Unknown";
    upgradeAvailable = data.isUpgradeAvailable || false;

    ifUpgradeAvailable();      
    updateUpgradeButtonState();

     // 🧠 Finalize
     console.log("✅ Configuration updated:", {
        moduleName, websiteName, smartboat, customMqtt, currentFW
    });

}

// --- UPGRADE FUNCTIONS

function handleUpgradeStatus(data) {
    console.log('Handling upgrade status:', data);
    firmwareVersionLabel.textContent = `Firmware Version - ${data.currentVersion}`;
    if (data.available) {
        upgradeButtonStart.disabled = false;  // Enable the upgrade button
        alert(`Upgrade available to version ${data.latestVersion}`);
    } else {
        upgradeButtonStart.disabled = true;  // Keep the upgrade button disabled
        console.log('⚠️ ALERT: No upgrade available!');
        alert('No upgrade available.');
    }
}

 
function showUpgradeProgress(data) {
    console.log('Showing upgrade progress...');
    otaProgressContainer.style.display = 'block';
}

function updateOtaProgress(data) {
    const progress = data.progress;
    progressBarFill.style.width = progress + '%';
    otaProgressText.textContent = `Upgrade Progress: ${progress}%`;

    if (progress >= 100) {
        setTimeout(() => {
            otaProgressContainer.style.display = 'none';
            alert('Upgrade completed successfully! You may need to refresh your browser.');
        }, 1000);
        closeUpgradePopup();
    } else if (progress === -1) {
        otaProgressText.textContent = "OTA failed. Please try again.";
        progressBarFill.style.backgroundColor = "red"; 
    }
    
}

function ifUpgradeAvailable(){
    if (upgradeAvailable) {
        document.querySelector('.notification-circle').style.display = 'block';
    } else {
        document.querySelector('.notification-circle').style.display = 'none';
    }
}

// POPUP FUNCTIONS

function openSettingsPopup() {
    
    // Set SmartBoat toggle based on the smartboat value
    if (smartboat) {
      document.getElementById('enableSmartBoat').checked = true;
    } else {
      document.getElementById('enableSmartBoat').checked = false;
    }
  
    // Set Custom MQTT toggle based on the customMqtt value
    if (customMqtt) {
      document.getElementById('enableCustomMqtt').checked = true;
    } else {
      document.getElementById('enableCustomMqtt').checked = false;
    }
  
    // Set the values for module name and website name
    document.getElementById('settingsModName').value = moduleName || "Smart Temperature Module";
    document.getElementById('settingsWebName').value = websiteName || "webpage";
  
    // Display the settings popup
    document.getElementById('settingsPop').style.display = 'flex';
  }
    
  function closeSettingsPopup() {
    // Ensure the popup element exists before trying to manipulate it
    var settingsPopup = document.getElementById('settingsPop');
    if (settingsPopup) {
      settingsPopup.style.display = 'none';
    } else {
      console.warn('Settings popup element with ID "settingsPop" not found.');
    }
  }
  
  // Function to handle the Custom MQTT toggle
  function toggleSmartBoatOrCustomMqtt(changedToggle) {
      var smartBoatToggle = document.getElementById('enableSmartBoat');
      var customMqttToggle = document.getElementById('enableCustomMqtt');
    
      if (changedToggle === 'smartBoat') {
        if (smartBoatToggle.checked) {
          customMqttToggle.checked = false;
        }
      } else if (changedToggle === 'customMqtt') {
        if (customMqttToggle.checked) {
          smartBoatToggle.checked = false;
          openCustomMqttPopup();  // Open popup for Custom MQTT
        }
      }
    }
  
  function confirmCustomMqtt() {
      // Get MQTT server and port values entered by the user
      customMQTTAddress = document.getElementById('mqttServer').value.trim();
      customMQTTPort = document.getElementById('mqttPort').value.trim();
      var customMqttEnabled = document.getElementById('enableCustomMqtt').checked;
  
      // Check if the entered MQTT server is a valid IP address
      if (!isValidIPAddress(customMQTTAddress)) {
          alert("Please enter a valid IP address for the MQTT server.");
          return;  // Stop further execution if the IP is not valid
      }
  
      // Update the variables but do NOT send data to the server yet
      window.customMQTTAddress = customMQTTAddress;
      window.customMQTTPort = customMQTTPort;
      window.customMqttEnabled = customMqttEnabled;
  
      // Close the Custom MQTT popup after confirmation
      closeCustomMqttPopup();
      confirmSmartboat();  // Assuming SmartBoat will also be confirmed
  }
  
    
    // Function to validate if the input is a valid IP address
    function isValidIPAddress(ip) {
      var regex = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
      return regex.test(ip);
    }
    
    // Attach the toggle event listeners
    document.getElementById('enableSmartBoat').addEventListener('change', function () {
      toggleSmartBoatOrCustomMqtt('smartBoat');
    });
    
    document.getElementById('enableCustomMqtt').addEventListener('change', function () {
      toggleSmartBoatOrCustomMqtt('customMqtt');
    });
  
  function openCustomMqttPopup() {
      document.getElementById('mqttServer').value = customMQTTAddress;
      document.getElementById('mqttPort').value = customMQTTPort;
      document.getElementById('customMqttPop').style.display = 'flex';
  }
    
    // Function to close the Custom MQTT popup
  function closeCustomMqttPopup() {
      document.getElementById('customMqttPop').style.display = 'none';
  }

  function openUpgradePopup() {
    console.log("Opening upgrade popup");

    const popup = document.getElementById("upgradePopup");
    if (popup) {
        popup.style.display = "flex";

        const upgradeCloseButton = document.getElementById('upgradeClose');
        if (upgradeCloseButton && !upgradeCloseButton.dataset.listener) {
            upgradeCloseButton.addEventListener('click', closeUpgradePopup);
            upgradeCloseButton.dataset.listener = "true";
            console.log("✅ Close button listener attached in openUpgradePopup()");
        }

        const firmwareVersionLabel = document.getElementById("firmwareVersionLabel");
        if (firmwareVersionLabel) {
            firmwareVersionLabel.textContent = `Firmware Version - ${currentFW}`;
        }

        const checkUpgradeButton = document.getElementById("checkUpgradeButton");
        if (checkUpgradeButton && !checkUpgradeButton.dataset.listener) {
            checkUpgradeButton.addEventListener("click", function () {
                console.log("Check for Upgrade button clicked");
                checkForUpgrade();
            });
            checkUpgradeButton.dataset.listener = "true";
        }

        const upgradeButtonStart = document.getElementById("upgradeButtonStart");
        if (upgradeButtonStart && !upgradeButtonStart.dataset.listener) {
            upgradeButtonStart.addEventListener("click", function () {
                console.log("Upgrade button clicked");
                initiateUpgrade();
            });
            upgradeButtonStart.dataset.listener = "true";
        }

        updateUpgradeButtonState(); // ✅ Ensure button matches upgradeAvailable flag
    } else {
        console.error("❌ Upgrade popup not found!");
    }
}    

function closeUpgradePopup(){
    console.log("👋 closeUpgradePopup called");
    document.getElementById('upgradePopup').style.display = 'none';
    closeSettingsPopup();
}

function initiateUpgrade() {
    console.log('Initiating upgrade...');

    // Example of sending a request to the server to initiate an upgrade
    const data ={
        type: 'startUpgrade'
    };

    sendDataToServer(data, false); // Expecting a response from the server
}

function checkForUpgrade() {
    console.log('Checking for upgrade...');

    const data = {
        type: 'checkForUpgrades'
    };

    sendDataToServer(data, true)
        .then(response => {
            console.log('🟢 Response received in checkForUpgrade:', response);
            handleUpgradeStatus(response);
        })
        .catch(err => {
            console.error('❌ Failed to check for upgrade:', err);
        });
}

function updateUpgradeButtonState() {
    console.log('update upgrade button state - upgrade available =', upgradeAvailable );
    const upgradeButtonEnable = document.getElementById('upgradeButtonStart');
    console.log('check button id exists');
    if (!upgradeButtonEnable) {
        console.error('Upgrade button not found.');
        return;
    }

    if (upgradeAvailable) {
        console.log('changing update button state to enabled');
        upgradeButtonEnable.disabled = false;
        upgradeButtonEnable.classList.remove('disabled');
    } else {
        upgradeButtonEnable.disabled = true;
        upgradeButtonEnable.classList.add('disabled');
    }
}

function confirmSmartboat() {
    smartBoat = true;
    //updateCheckboxState();
    console.log('SmartBoat enabled');
    closeSmartboatPopup();
    closeSettingsPopup();
    sendSettingsToServer();
}

function setSettings() {
    // Get values from input fields
    var moduleNameNew = document.getElementById('settingsModName').value.trim();
    var websiteNameNew = document.getElementById('settingsWebName').value.trim();
    var smartboat = document.getElementById('enableSmartBoat').checked ? 1 : 0;
    var customMqtt = document.getElementById('enableCustomMqtt').checked ? 1 : 0;

    // Update displayed module name immediately
    var modNameElement = document.getElementById('modName');
    if (modNameElement) {
      modNameElement.textContent = moduleNameNew;
    }

    // Update local variables (but do not send to server yet)
    moduleName = moduleNameNew;
    websiteName = websiteNameNew;

    // If either SmartBoat or Custom MQTT is enabled, show the confirmation popup
    if (smartboat || customMqtt) {
      document.getElementById('smartboatPop').style.display = 'flex';
      return; // Halt further execution until the user confirms via the popup
    }

    // If neither SmartBoat nor Custom MQTT is enabled, proceed with sending settings
    proceedWithSettingsUpdate(moduleName, websiteName, smartboat, customMqtt);
}

function proceedWithSettingsUpdate(moduleName, websiteName, smartboat, customMqtt) {
    // Prepare the settings data to send to the server
    var settings = {
      type: "updateSettings",
      moduleName: moduleName,
      webname: websiteName,
      smartboat: smartboat,
      customMqtt: customMqtt,
      customMqttServer: customMQTTAddress,
      customMqttPort: customMQTTPort
    };

    // Send the settings using the sendDataToServer function (assuming you have this function)
    sendDataToServer(settings);

    // Update the UI elements with the new settings
    var modNameElement = document.getElementById('modName');
    if (modNameElement) {
      modNameElement.textContent = moduleName;
    }

    var websiteNameElement = document.getElementById('websiteName');
    if (websiteNameElement) {
      websiteNameElement.textContent = websiteName;
    }

    var smartboatStatusElement = document.getElementById('smartboatStatus');
    if (smartboatStatusElement) {
      smartboatStatusElement.textContent = smartboat ? 'Enabled' : 'Disabled';
    }

    var customMqttStatusElement = document.getElementById('customMqttStatus');
    if (customMqttStatusElement) {
      customMqttStatusElement.textContent = customMqtt ? 'Enabled' : 'Disabled';
    }

    console.log('Settings updated and saved to the server.');

    // Close the settings popup
    closeSettingsPopup();
}

function openRestartPopup(stripNumber) {
    pendingStripNumber = stripNumber;
    document.getElementById('restartPop').style.display = 'block';
    document.body.style.overflow = 'hidden';
}

function closeRestartPopup() {
    document.getElementById('restartPop').style.display = 'none';
    document.body.style.overflow = '';
}

function confirmRestart() {
    closeRestartPopup();
    setLedStripSettings(pendingStripNumber); // Use the stored strip number
}

function openResetPopup() {
    document.getElementById('resetPop').style.display = 'flex';
    document.body.style.overflow = 'hidden';
}

function closeResetPopup() {
    document.getElementById('resetPop').style.display = 'none';
    document.body.style.overflow = '';
    closeSettingsPopup();
}

function confirmReset() {
    console.log('⚠️ Reset confirmed – sending command to server...');
    closeResetPopup();

    sendDataToServer({
        action: "reset",
        requestId: `req-reset-${Date.now()}`
    })
    .then(() => {
        console.log("✅ Reset command sent successfully.");
    })
    .catch((err) => {
        console.error("❌ Failed to send reset command:", err);
        alert("⚠️ Reset failed to send. Please check your connection.");
    });
}

function openSmartboatPopup() {
    document.getElementById('smartboatPop').style.display = 'block';
    document.body.style.overflow = 'hidden';
}

function closeSmartboatPopup() {
    document.getElementById('smartboatPop').style.display = 'none';
    document.body.style.overflow = '';
}

function sendSettingsToServer() {
    const moduleName = document.getElementById('settingsModName').value;
    const websiteURL = document.getElementById('settingsWebName').value;

    // Convert the MQTT port value to a number
    const customMqttPort = parseInt(document.getElementById('mqttPort').value.trim());

    // Construct the settings object
    const settings = {
        type: 'settingsUpdate',
        moduleName: moduleName,
        websiteURL: websiteURL,
        smartboat: smartBoat,  // Corrected to match the lowercase key: 'smartboat'
        customMqtt: customMqtt,
        customMqttServer: customMQTTAddress,
        customMqttPort: customMqttPort  // Ensure this is a number, not a string
    };


    // Send the data to the server (assuming sendDataToServer is a function that sends the data)
    sendDataToServer(settings);

    console.log("Settings updated and sent to server:", jsonString);

    // Optionally, you can update the UI with the new settings
    var modNameElement = document.getElementById('modName');
    if (modNameElement) {
        modNameElement.textContent = moduleName;
    }

    var websiteNameElement = document.getElementById('websiteName');
    if (websiteNameElement) {
        websiteNameElement.textContent = websiteURL;
    }

    var smartboatStatusElement = document.getElementById('smartboatStatus');
    if (smartboatStatusElement) {
        smartboatStatusElement.textContent = smartBoat ? 'Enabled' : 'Disabled';
    }

    var customMqttStatusElement = document.getElementById('customMqttStatus');
    if (customMqttStatusElement) {
        customMqttStatusElement.textContent = customMqtt ? 'Enabled' : 'Disabled';
    }

    console.log('Settings updated and saved to the server.');

    // Close the settings popup
    closeSettingsPopup();
}

async function loadModuleHTML() {
    try {
        const response = await fetch("module.html");
        const html = await response.text();

        const tempDiv = document.createElement("div");
        tempDiv.innerHTML = html;

        // Grab parts and inject
        const settings = tempDiv.querySelector(".module-settings-extension");
        const popups = tempDiv.querySelector(".module-popup-extension");
        const controls = tempDiv.querySelector(".module-control-extension");

        if (settings) document.getElementById("moduleSettingsArea").appendChild(settings);
        if (popups) document.getElementById("modulePopupArea").appendChild(popups);
        if (controls) document.getElementById("moduleControlArea").appendChild(controls);

        console.log("✅ Module HTML loaded and injected");
    } catch (err) {
        console.error("❌ Failed to load module.html", err);
    }
}

  
document.addEventListener('DOMContentLoaded', async function () {
    console.log('✅ DOM fully loaded and parsed');

    // Load and inject module-specific HTML
    await loadModuleHTML();

    // Run module-specific logic if provided
    if (typeof moduleJS === "function") {
        console.log("📦 Running module-specific JS setup...");
        moduleJS();
    }

    // Cache key DOM elements
    const otaProgressContainer = document.getElementById('otaProgressContainer');
    const progressBarFill = document.getElementById('progressBarFill');
    const otaProgressText = document.getElementById('otaProgressText');

    // WebSocket setup and UI state
    setupWebSocket();
    updateUpgradeButtonState();
    ifUpgradeAvailable();

    // Upgrade buttons
    const checkUpgradeButton = document.getElementById('checkUpgradeButton');
    if (checkUpgradeButton) {
        checkUpgradeButton.addEventListener('click', function () {
            const upgradeRequest = { type: "checkForUpgrades" };
            ws.send(JSON.stringify(upgradeRequest));
        });
    }

    const upgradeButtonStart = document.getElementById('upgradeButtonStart');
    if (upgradeButtonStart) {
        upgradeButtonStart.addEventListener('click', function () {
            const upgradeStart = { type: "startUpgrade" };
            ws.send(JSON.stringify(upgradeStart));
        });
    }

    const upgradeClose = document.getElementById('upgradeClose');
    if (upgradeClose) {
        upgradeClose.addEventListener('click', function () {
            const upgradePopup = document.getElementById('upgradePopup');
            if (upgradePopup) upgradePopup.style.display = 'none';
        });
    }

    const upgradeOpenButton = document.querySelector('.upgrade-btn');
    if (upgradeOpenButton) {
        upgradeOpenButton.addEventListener('click', openUpgradePopup);
    }

    // Settings popup buttons
    const settingsButton = document.querySelector('.settings-button');
    if (settingsButton) {
        settingsButton.addEventListener('click', openSettingsPopup);
    }

    const settingsCloseButton = document.querySelector('#settingsPop .close');
    if (settingsCloseButton) {
        settingsCloseButton.addEventListener('click', closeSettingsPopup);
    }

    const setButton = document.querySelector('.set-btn');
    if (setButton) {
        setButton.addEventListener('click', setSettings);
    }

    const resetButton = document.querySelector('.reset-btn');
    if (resetButton) {
        resetButton.addEventListener('click', openResetPopup);
    }

});


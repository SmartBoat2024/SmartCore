let blocksData = [];
    let currentBlockIndex = 0;
    let currentBlockDisplayIndex = 0;
    let currentI2CAddress = null;
    let testSensorData = {};
    let voltmetersData = [];
    let currentVoltmeterIndex = 0;
    let voltmeterConfigMode = "4V";
    let previousVoltmeterConfigMode = "4V"; 
    let shuntData = [];
    let alertsSilenced = false;
    const backendHost = "smartmodule.local";
    let frameRightRendered = false;
    let reconnectInProgress = false;
    let configWizardActive = false;
    let currentRelayIndex = 0;
    let relayFormChanged = false;
    let relaysData = [];
    let selectedSourceType = "block";
    let selectedSourceName = "";
    let relayConfigs = [];
    let nameField, locationField, onTime, offTime, repeatCount;
    let cycleEnabled, relayStateDropdown, relayEnabledCheckbox;
    let batteryCapacityAh = 500;
    let selectedBatteryType = "lead";
    const batteryProfiles = {
        lead: {
          name: "Lead Acid",
          bulk: 14.4,
          absorb: 14.2,
          float: 13.5,
          absorbCurrent: 0.05
        },
        agm: {
          name: "AGM",
          bulk: 14.6,
          absorb: 14.4,
          float: 13.6,
          absorbCurrent: 0.04
        },
        gel: {
          name: "Gel",
          bulk: 14.1,
          absorb: 14.1,
          float: 13.5,
          absorbCurrent: 0.03
        },
        lifepo4: {
          name: "LiFePO₄",
          bulk: 14.6,
          absorb: 14.4,
          float: 13.6,
          absorbCurrent: 0.02
        },
        custom: {
          name: "Custom",
          bulk: "0.0",
          absorb: "0.0",
          float: "0.0",
          absorbCurrent: "0.0"
        }
      };

function openVoltmeterSetupPopup() {
    console.log("📡 Opening Voltmeter Setup Popup...");

    currentVoltmeterIndex = 0;

    // Only generate popup structure here
    generateVoltmeterPopup();

    // Let navigateVoltmeter handle logic cleanly
    setTimeout(() => {
        navigateVoltmeter(0); // Will call the correct populate function based on mode
    }, 50);

    document.getElementById("voltmeterSetupPop").style.display = "flex";
}

function enableVoltmeterListeners() {
    console.log("🔄 Adding change listeners to voltmeter form fields...");

    const setVoltmeterBtn = document.getElementById("setVoltmeterBtn");
    if (!setVoltmeterBtn) {
        console.error("❌ 'Set Voltmeter' button not found!");
        return;
    }

    // ✅ Show alert
    function showAlert(message) {
        alert(message);
}

// ✅ Allow only numeric input and one decimal point
function enforceNumericInput(event) {
    const field = event.target;
    let cleaned = field.value.replace(/[^0-9.]/g, "");
    const parts = cleaned.split(".");
    if (parts.length > 2) {
        cleaned = parts[0] + "." + parts[1]; // Only keep one dot
    }
    field.value = cleaned;
}

// ✅ Format number on blur to 1 decimal place
function formatToOneDecimal(event) {
    const value = parseFloat(event.target.value);
    if (!isNaN(value)) {
        event.target.value = value.toFixed(1);
    }
}

// ✅ Validate all fields and enable/disable Set button
function validateAndEnableSetButton(event) {
    const voltmeterNameField = document.getElementById("voltmeterName");
    const minVoltageField = document.getElementById("voltageMin");
    const maxVoltageField = document.getElementById("voltageMax");
    const systemVoltage = document.querySelector('input[name="voltmeterVoltage"]:checked')?.value || "12";
    const maxAllowedVoltage = systemVoltage === "12" ? 15.0 : 32.0;

    const voltmeterName = voltmeterNameField.value.trim();
    let minVoltage = parseFloat(minVoltageField.value);
    let maxVoltage = parseFloat(maxVoltageField.value);
    let isValid = true;

    if (voltmeterName.length > 16) {
        voltmeterNameField.value = voltmeterName.substring(0, 16);
        showAlert("⚠️ Voltmeter name cannot exceed 16 characters!");
        isValid = false;
    }

    if (minVoltageField.value.trim() === "" || maxVoltageField.value.trim() === "") {
        setVoltmeterBtn.disabled = true;
        return;
    }

    if (isNaN(minVoltage)) {
        minVoltageField.value = "";
        isValid = false;
    }

    if (isNaN(maxVoltage)) {
        maxVoltageField.value = "";
        isValid = false;
    }

    if (event.type === "input") {
        if (minVoltage < 0) minVoltageField.value = "0";
        if (maxVoltage > maxAllowedVoltage) maxVoltageField.value = maxAllowedVoltage.toFixed(1);
    }

    if (!isNaN(minVoltage) && minVoltage < 0) {
        showAlert("⚠️ Min voltage must be at least 0V!");
        isValid = false;
    }

    if (!isNaN(maxVoltage) && maxVoltage > maxAllowedVoltage) {
        showAlert(`⚠️ Max voltage cannot exceed ${maxAllowedVoltage}V for a ${systemVoltage}V system!`);
        isValid = false;
    }

    setVoltmeterBtn.disabled = !isValid;
    }

    // ✅ Attach listeners
    const minField = document.getElementById("voltageMin");
    const maxField = document.getElementById("voltageMax");
    const voltName = document.getElementById("voltmeterName");

    voltName.addEventListener("input", validateAndEnableSetButton);
    minField.addEventListener("input", enforceNumericInput);
    minField.addEventListener("input", validateAndEnableSetButton);
    minField.addEventListener("blur", formatToOneDecimal);
    minField.addEventListener("change", validateAndEnableSetButton);
    maxField.addEventListener("input", enforceNumericInput);
    maxField.addEventListener("input", validateAndEnableSetButton);
    maxField.addEventListener("blur", formatToOneDecimal);
    maxField.addEventListener("change", validateAndEnableSetButton);

    document.getElementById("minVoltageAlert").addEventListener("change", validateAndEnableSetButton);
    document.getElementById("maxVoltageAlert").addEventListener("change", validateAndEnableSetButton);

    document.querySelectorAll('input[name="voltmeterVoltage"]').forEach(radio => {
        radio.addEventListener("change", (event) => {
            const systemVoltage = event.target.value;
            const maxVoltageField = document.getElementById("voltageMax");
            const currentMax = parseFloat(maxVoltageField.value);

            if (systemVoltage === "12" && currentMax > 15) {
                console.log("⚙️ Auto-setting max voltage to 15 for 12V system");
                maxVoltageField.value = "15";
            } else if (systemVoltage === "24" && currentMax < 15.1) {
                console.log("⚙️ Auto-setting max voltage to 32 for 24V system");
                maxVoltageField.value = "32";
            }

            validateAndEnableSetButton(event);
        });
    });

console.log("✅ Voltmeter listeners attached successfully.");
}

function populateVoltmeterDetails(voltmeter) {
    if (!voltmeter) {
        console.error("⚠️ No voltmeter data found!");
        return;
    }

    console.log(`🔍 Loading Voltmeter Data:`, voltmeter);

    const voltmeterNameField = document.getElementById("voltmeterName");
    const voltageMinField = document.getElementById("voltageMin");
    const voltageMaxField = document.getElementById("voltageMax");
    const minVoltageAlertField = document.getElementById("minVoltageAlert");
    const maxVoltageAlertField = document.getElementById("maxVoltageAlert");

    if (!voltmeterNameField || !voltageMinField || !voltageMaxField || !minVoltageAlertField || !maxVoltageAlertField) {
        console.error("❌ Missing one or more expected elements in the DOM.");
        return;
    }

    // ✅ Populate form fields
    voltmeterNameField.value = voltmeter.voltmeterName;
    voltageMinField.value = voltmeter.minVoltage;
    voltageMaxField.value = voltmeter.maxVoltage;

    const systemVoltage = voltmeter.systemVoltage || 24;
    const voltageRadio = document.querySelector(`input[name="voltmeterVoltage"][value="${systemVoltage}"]`);
    if (voltageRadio) voltageRadio.checked = true;

    // ✅ Use flat properties instead of nested alerts
    minVoltageAlertField.checked = voltmeter.alerts?.minVoltage ?? false;
    maxVoltageAlertField.checked = voltmeter.alerts?.maxVoltage ?? false;
}

function generateVoltmeterPopup() {
    console.log("🛠 Generating Voltmeter Setup Popup...");

    const popupContainer = document.getElementById("voltmeterSetupPop");
    popupContainer.innerHTML = ""; // Clear previous content

    // ✅ Create popup structure
    const popupContent = document.createElement("div");
    popupContent.className = "popup-content";

    popupContent.innerHTML = `
        <span class="close" onclick="closeVoltmeterSetupPopup()">&times;</span>
        <h2>Voltmeter/Shunt Setup</h2>

        <!-- Voltmeter Counter -->
        <div id="voltmeterCounter" class="voltmeter-counter">1/4</div>

        <!-- Dynamic Content Area -->
        <div id="voltmeterFormContainer"></div>

        <!-- Navigation Buttons -->
        <div class="button-group">
            <button id="prevVoltmeterBtn" onclick="navigateVoltmeter(-1)">Previous</button>
            <button id="nextVoltmeterBtn" onclick="navigateVoltmeter(1)">Next</button>
        </div>

        <!-- Another Horizontal line -->
        <div class="horizontal-line"></div>

        <!-- Action Buttons -->
        <div class="button-group">
            <button id="wizardBtn" class="wizard-voltmeter-btn" onclick="openVoltageSetupWizard()">Wizard</button>
            <button id="setVoltmeterBtn" class="edit-voltmeter-btn" disabled onclick="setVoltmeterConfiguration()">Set</button>
        </div>
    `;

    // ✅ Append dynamically created content to the popup container
    popupContainer.appendChild(popupContent);

    console.log("✅ Voltmeter Setup Popup generated successfully.");

    // Populate the first entry based on the mode
    navigateVoltmeter(0);
}

function generateVoltmeterForm() {
    return `
        <!-- Voltmeter Name -->
        <div class="form-group">
            <label for="voltmeterName">Voltmeter Name:</label>
            <input type="text" id="voltmeterName" name="voltmeterName" value="">
        </div>

        <!-- Voltage Selection (12V / 24V) -->
        <div class="form-group">
            <label>Voltage:</label>
            <div class="radio-group-voltage">
                <label><input type="radio" name="voltmeterVoltage" value="12"> 12V</label>
                <label><input type="radio" name="voltmeterVoltage" value="24"> 24V</label>
            </div>
        </div>

        <div class="horizontal-line"></div>

        <!-- Min Voltage Input (Inline with Alert Checkbox) -->
        <div class="voltage-input-container">
            <label for="voltageMin">Min:</label>
            <input type="number" id="voltageMin" min="0" max="32" step="0.1" value="0">
            <label for="minVoltageAlert">Alert:</label>
            <input type="checkbox" id="minVoltageAlert">
        </div>

        <!-- Max Voltage Input (Inline with Alert Checkbox) -->
        <div class="voltage-input-container">
            <label for="voltageMax">Max:</label>
            <input type="number" id="voltageMax" min="0" max="32" step="0.1" value="32">
            <label for="maxVoltageAlert">Alert:</label>
            <input type="checkbox" id="maxVoltageAlert">
        </div>

        <div class="horizontal-line"></div>
    `;
}

function generateShuntForm() {
    return `
        <!-- SmartShunt Checkbox -->
           
        <!-- Shunt Name -->
        <div class="form-group">
            <label for="shuntName">Shunt Name:</label>
            <input type="text" id="shuntName" name="shuntName" value="">
        </div>

        <div class="horizontal-line"></div>

       
        <!-- Millivolt Selection -->
        <div class="form-group">
            <label>Shunt Millivolt Rating:</label>
             <div class="form-group">
                <label><input type="checkbox" id="smartShuntCheckbox" checked>SmartShunt Installed</label>
            </div>
            <div class="radio-group">
                <label><input type="radio" name="shuntMV" value="50" checked disabled> 50mV (Default)</label>
                <label><input type="radio" name="shuntMV" value="75" disabled> 75mV</label>
                <label><input type="radio" name="shuntMV" value="100" disabled> 100mV</label>
                <label><input type="radio" name="shuntMV" value="custom" disabled> Custom:</label>
                <input type="number" id="customShuntMV" min="0" max="200" step="0.1" value="50" disabled>
            </div>
        </div>

        <div class="horizontal-line"></div>

        <!-- Voltage Selection -->
        <div class="form-group">
            <label>Voltage:</label>
            <div class="radio-group-shunt">
                <label><input type="radio" name="shuntVoltage" value="12" checked> 12V</label>
                <label><input type="radio" name="shuntVoltage" value="24"> 24V</label>
            </div>
        </div>

        <div class="horizontal-line"></div>

        <!-- State of Charge (SOC) Section -->
        <div class="form-group">
            <label>State of Charge (SOC) Settings:</label>
            <div class="checkbox-group">
                <label><input type="checkbox" id="autoSOCCheckbox" checked> Auto (Default)</label>
            </div>
            <button id="resetSOCBtn" onclick="resetSOC()">Reset SOC to 100%</button>
            <button id="batterySetupBtn" onclick="openBatterySetupPopup()">Set Battery Parameters</button>
        </div>

        <div class="horizontal-line"></div>

        <!-- Min SOC Input & Alert Checkbox -->
        <div class="form-group soc-inline">
            <label for="minSOC">Min SOC:</label>
            <input type="number" id="minSOC" min="0" max="100" step="1" value="20">
            <label for="minSOCAlert">Alert:</label>
            <input type="checkbox" id="minSOCAlert">
        </div>

        <div class="horizontal-line"></div>
    `;
}

function enableShuntListeners() {
    console.log("🔄 Adding event listeners for Shunt Setup...");

    const smartShuntCheckbox = document.getElementById("smartShuntCheckbox");
    const shuntMVOptions = document.querySelectorAll('input[name="shuntMV"]');
    const customShuntMV = document.getElementById("customShuntMV");
    const minSOCField = document.getElementById("minSOC");
    const setVoltmeterBtn = document.getElementById("setVoltmeterBtn");

    if (!setVoltmeterBtn) {
        console.error("❌ 'Set' button not found!");
        return;
    }

    // ✅ Function to validate and enable Set button
    function validateAndEnableSetButton(event) {
        let isValid = true;

        // ✅ Limit Shunt Name to 16 Characters
        const shuntNameField = document.getElementById("shuntName");
        if (shuntNameField) {
            const shuntName = shuntNameField.value.trim();
            if (shuntName.length > 16) {
                shuntNameField.value = shuntName.substring(0, 16);
                alert("⚠️ Shunt name cannot exceed 16 characters!");
                isValid = false;
            }
        }

        // ✅ Ensure Only Numbers in SOC & Custom mV Fields
        minSOCField.value = minSOCField.value.replace(/[^0-9]/g, "");  // SOC should be whole numbers only
        customShuntMV.value = customShuntMV.value.replace(/[^0-9.]/g, ""); // Custom mV should allow decimal values

        // ✅ Check Min SOC (Allow Empty Field Without Warning)
        let minSOC = minSOCField.value.trim();
        if (minSOC === "") {
            setVoltmeterBtn.disabled = true;
            return; // Allow user to type before validation
        }

        minSOC = parseInt(minSOC);
        if (!isNaN(minSOC) && (minSOC < 0 || minSOC > 100)) {
            alert("⚠️ Min SOC must be between 0% and 100%!");
            minSOCField.value = "20"; // Reset to 20% if invalid
            isValid = false;
        }

        // ✅ Check Custom mV (Fixed Issue)
        if (document.querySelector('input[name="shuntMV"][value="custom"]').checked) {
            let customMV = customShuntMV.value.trim();

            // 🛑 FIXED: Allow user to delete & re-enter a value without warnings
            if (customMV === "") {
                setVoltmeterBtn.disabled = true;
                return; // Stop further validation until user finishes typing
            }

            customMV = parseFloat(customMV);
            if (!isNaN(customMV) && customMV > 200) {
                alert("⚠️ Custom shunt millivolt rating cannot exceed 200mV!");
                customShuntMV.value = "200"; // Reset to 200mV if exceeded
                isValid = false;
            }

            if (!isNaN(customMV) && customMV < 20) {
                setVoltmeterBtn.disabled = true;
                return; // Disable "Set" button but no warning for values below 50
            }
        }

        // ✅ Enable Set button only if all conditions pass
        setVoltmeterBtn.disabled = !isValid;
    }

    // ✅ SmartShunt Checkbox Change Listener
    smartShuntCheckbox.addEventListener("change", function () {
        const isChecked = this.checked;

        // Enable or disable the millivolt options based on SmartShunt selection
        shuntMVOptions.forEach(option => {
            option.disabled = isChecked;
        });

        // Reset to 50mV default if enabling SmartShunt
        if (isChecked) {
            document.querySelector('input[name="shuntMV"][value="50"]').checked = true;
            customShuntMV.disabled = true;
        } else {
            customShuntMV.disabled = !document.querySelector('input[name="shuntMV"][value="custom"]').checked;
        }

        validateAndEnableSetButton();
    });

    // ✅ Custom mV Selection Change Listener
    document.querySelector('input[name="shuntMV"][value="custom"]').addEventListener("change", function () {
        customShuntMV.disabled = !this.checked;
        validateAndEnableSetButton();
    });

    // ✅ Attach validation to input fields
    const shuntInputs = ["shuntName", "customShuntMV", "autoSOCCheckbox", "minSOC", "minSOCAlert"];
    shuntInputs.forEach(id => {
        const inputField = document.getElementById(id);
        if (inputField) {
            inputField.addEventListener("input", validateAndEnableSetButton);
            inputField.addEventListener("change", validateAndEnableSetButton);
        } else {
            console.warn(`⚠️ Missing expected input field: #${id}`);
        }
    });

    // ✅ Add listeners for Shunt Voltage Selection (12V / 24V)
    const voltageRadios = document.querySelectorAll('input[name="shuntVoltage"]');
    voltageRadios.forEach(radio => {
        radio.addEventListener("change", validateAndEnableSetButton);
    });

    console.log("✅ Shunt listeners attached successfully.");
}

function resetSOC() {
    console.log("⚡ Sending SOC reset command to server...");

    let shuntIndex = -1;

    if (voltmeterConfigMode === "2S") {
        shuntIndex = currentVoltmeterIndex;
    } else if (voltmeterConfigMode === "1S2V") {
        shuntIndex = 0;
    } else if (voltmeterConfigMode === "2V1S") {
        shuntIndex = currentVoltmeterIndex - 2;
    }

    if (shuntIndex < 0 || !shuntData[shuntIndex]) {
        console.error("❌ No valid shunt data found for SOC reset.");
        return;
    }

    const shuntId = shuntData[shuntIndex].id;

    sendDataToServer({
        command: "resetSOC",
        shuntId: shuntId,
        requestId: `req-resetSOC-${Date.now()}`
    })
    .then(() => {
        console.log(`✅ SOC reset command sent successfully for Shunt ID: ${shuntId}`);
        alert("✅ State of Charge (SOC) reset command has been sent to the server.");
    })
    .catch((error) => {
        console.error("❌ Error sending SOC reset command:", error);
        alert("⚠️ Failed to send SOC reset command. Please check your connection.");
    });
}    
   
function navigateVoltmeter(direction) {
    const mode = voltmeterConfigMode;
    let totalEntries = 4; // Default (4V mode)

    if (mode === "2V1S" || mode === "1S2V") {
        totalEntries = 3;
    } else if (mode === "2S") {
        totalEntries = 2;
    }

    const newIndex = currentVoltmeterIndex + direction;

    if (newIndex >= 0 && newIndex < totalEntries) {
        currentVoltmeterIndex = newIndex;
        const formContainer = document.getElementById("voltmeterFormContainer");

        const isShunt = (
            (mode === "1S2V" && newIndex === 0) ||
            (mode === "2V1S" && newIndex === 2) ||
            (mode === "2S")
        );

        if (isShunt) {
            // Show Shunt Form
            formContainer.innerHTML = generateShuntForm();

            let shuntIndex = 0;
            if (mode === "2S") {
                shuntIndex = newIndex;
            } else if (mode === "2V1S") {
                shuntIndex = 0; // 2V1S only has 1 shunt at index 0
            } else if (mode === "1S2V") {
                shuntIndex = 0; // 1S2V also has 1 shunt at index 0
            }

            const shunt = shuntData[shuntIndex] || getDefaultShunt(shuntIndex);
            populateShuntDetails(shunt);
            enableShuntListeners();
        } else {
            // Show Voltmeter Form
            formContainer.innerHTML = generateVoltmeterForm();

            // ✅ Map popup index to voltmeterData index
            let voltIndex = newIndex;
            if (mode === "1S2V") voltIndex = newIndex - 1;
            if (mode === "2V1S") voltIndex = newIndex;

            const voltmeter = voltmetersData[voltIndex] || getDefaultVoltmeter(voltIndex);
            populateVoltmeterDetails(voltmeter);
            enableVoltmeterListeners();
        }

        updateVoltmeterCounter(totalEntries);
    } else {
        console.warn("🚫 Navigation limit reached.");
    }
}    

function populateShuntDetails(shunt) {
    if (!shunt) {
        console.error("⚠️ No shunt data found!");
        return;
    }

    console.log(`****************💡 Final shuntName in form: ${shunt.shuntName}**********************`);

    console.log(`🔍 Loading Shunt Data:`, shunt);

    // Safe value extraction
    const shuntName = shunt.shuntName || "New Shunt";
    const smartShunt = shunt.smartShunt ?? true;
    const shuntMV = shunt.shuntMV || "50";
    const customShuntMVVal = shunt.customShuntMV || "50";
    const systemVoltage = shunt.systemVoltage || "12";
    const autoSOC = shunt.autoSOC ?? true;
    const minSOC = shunt.minSOC ?? 20;
    const minSOCAlert = (shunt.alerts && typeof shunt.alerts.minSOC !== "N/A")
        ? shunt.alerts.minSOC
        : false;

    // DOM elements
    const shuntNameField = document.getElementById("shuntName");
    const smartShuntCheckbox = document.getElementById("smartShuntCheckbox");
    const shuntMVOptions = document.querySelectorAll('input[name="shuntMV"]');
    const customShuntMV = document.getElementById("customShuntMV");
    const voltageRadios = document.querySelectorAll('input[name="shuntVoltage"]');
    const autoSOCCheckbox = document.getElementById("autoSOCCheckbox");
    const minSOCField = document.getElementById("minSOC");
    const minSOCAlertField = document.getElementById("minSOCAlert");

    if (!shuntNameField || !smartShuntCheckbox || !shuntMVOptions.length || 
        !customShuntMV || !voltageRadios.length || !autoSOCCheckbox || !minSOCField || !minSOCAlertField) {
        console.error("❌ Critical missing DOM elements.");
        return;
    }

    // Apply values
    shuntNameField.value = shuntName;
    smartShuntCheckbox.checked = smartShunt;
    autoSOCCheckbox.checked = autoSOC;
    minSOCField.value = minSOC;
    minSOCAlertField.checked = minSOCAlert;

    if (smartShunt) {
        shuntMVOptions.forEach(option => option.disabled = true);
        const defaultMV = document.querySelector('input[name="shuntMV"][value="50"]');
        if (defaultMV) defaultMV.checked = true;
        customShuntMV.disabled = true;
    } else {
        shuntMVOptions.forEach(opt => opt.disabled = false);
        const selectedMV = document.querySelector(`input[name="shuntMV"][value="${shuntMV}"]`);
        if (selectedMV) selectedMV.checked = true;

        customShuntMV.disabled = (shuntMV !== "custom");
        if (shuntMV === "custom") {
            customShuntMV.value = customShuntMVVal;
        }
    }

    const selectedVoltage = document.querySelector(`input[name="shuntVoltage"][value="${systemVoltage}"]`);
    if (selectedVoltage) selectedVoltage.checked = true;
}    

function getDefaultVoltmeter(index) {
    return {
        id: index + 1,
        voltmeterName: `Voltmeter ${index + 1}`,
        systemVoltage: 12, // Default to 12V
        minVoltage: 10.5,  // Default min voltage
        maxVoltage: 15.0,  // Default max voltage
        alerts: {
            minVoltage: false,
            maxVoltage: false
        }
    };
}

function getDefaultShunt(index) {
    return {
        id: index + 1,
        shuntName: `Shunt ${index + 1}`,
        smartShunt: true,  // Default to SmartShunt enabled
        shuntMV: "50", // Default to 50mV
        customShuntMV: 50,
        systemVoltage: 12, // Default to 12V
        autoSOC: true, // Default to Auto SOC enabled
        minSOC: 20, // Default Min SOC alert threshold
        alerts: {
            minSOC: false
        }
    };
}


function updateVoltmeterCounter(totalEntries) {
    document.getElementById("voltmeterCounter").textContent = `${currentVoltmeterIndex + 1}/${totalEntries}`;

    // Enable/Disable navigation buttons
    document.getElementById("prevVoltmeterBtn").disabled = currentVoltmeterIndex === 0;
    document.getElementById("nextVoltmeterBtn").disabled = currentVoltmeterIndex === totalEntries - 1;
}


function setVoltmeterConfiguration() {
    console.log("⚙️ Updating Voltmeter/Shunt Configuration...");

    const setVoltmeterBtn = document.getElementById("setVoltmeterBtn");
    if (!setVoltmeterBtn) {
        console.error("❌ 'Set' button not found!");
        return;
    }

    const configModePayload = {
        command: 'updateConfigMode',
        configMode: voltmeterConfigMode
    };

    // First send config mode and wait for response
    sendDataToServer(configModePayload, true)
        .then(() => {
            console.log("✅ Config Mode successfully sent to server.");
            previousVoltmeterConfigMode = voltmeterConfigMode;

            // Now prepare voltmeter or shunt update
            let updatedData = {};
            const isShunt = (voltmeterConfigMode === "2V1S" && currentVoltmeterIndex === 2) ||
                            (voltmeterConfigMode === "1S2V" && currentVoltmeterIndex === 0) ||
                            (voltmeterConfigMode === "2S");

            if (isShunt) {
                console.log("🔧 Configuring Shunt...");
            
                const shuntNameInput = document.getElementById("shuntName");
                const smartShuntCheckbox = document.getElementById("smartShuntCheckbox");
                const selectedShuntMV = document.querySelector('input[name="shuntMV"]:checked');
                const customShuntMVInput = document.getElementById("customShuntMV");
                const selectedVoltageRadio = document.querySelector('input[name="shuntVoltage"]:checked');
                const autoSOCCheckbox = document.getElementById("autoSOCCheckbox");
                const minSOCInput = document.getElementById("minSOC");
                const minSOCAlertCheckbox = document.getElementById("minSOCAlert");
            
                if (!shuntNameInput || !selectedShuntMV || !selectedVoltageRadio || !minSOCInput) {
                    console.error("❌ Error: One or more required shunt inputs are missing.");
                    return;
                }
            
                const shuntName = shuntNameInput.value.trim();
                const smartShunt = smartShuntCheckbox.checked;
                const shuntMV = selectedShuntMV.value === "custom"
                    ? parseFloat(customShuntMVInput.value)
                    : parseInt(selectedShuntMV.value);
                const systemVoltage = parseInt(selectedVoltageRadio.value);
                const autoSOC = autoSOCCheckbox.checked;
                const minSOC = parseInt(minSOCInput.value);
                const minSOCAlert = minSOCAlertCheckbox.checked;
            
                let shuntIndex = -1;
                if (voltmeterConfigMode === "1S2V") shuntIndex = 0;
                else if (voltmeterConfigMode === "2V1S") shuntIndex = 0;
                else if (voltmeterConfigMode === "2S") shuntIndex = currentVoltmeterIndex;
            
                while (shuntData.length <= shuntIndex) shuntData.push({});
            
                // ✅ Battery config
                const batteryConfig = {
                    type: selectedBatteryType,
                    capacity: batteryCapacityAh,
                    profile: selectedBatteryType === "custom" ? { ...batteryProfiles.custom } : null
                };
            
                // ✅ Shunt data object
                const shuntToSend = {
                    id: shuntData[shuntIndex]?.id || (shuntIndex + 1),
                    shuntName,
                    smartShunt,
                    shuntMV,
                    systemVoltage,
                    autoSOC,
                    minSOC,
                    alerts: { minSOC: minSOCAlert },
                    battery: batteryConfig
                };
            
                updatedData = {
                    command: 'updateShuntConfiguration',
                    shuntData: shuntToSend,
                    configMode: voltmeterConfigMode
                };
            
                console.log(`✅ Shunt updated:`, shuntToSend);
            
                shuntData[shuntIndex] = {
                    ...shuntData[shuntIndex], // preserve existing props if needed
                    ...shuntToSend            // overwrite with new data
                };
            } else {
                console.log("⚙️ Configuring Voltmeter...");

                const voltmeterNameInput = document.getElementById('voltmeterName');
                const voltageMinInput = document.getElementById('voltageMin');
                const voltageMaxInput = document.getElementById('voltageMax');
                const minVoltageAlertCheckbox = document.getElementById('minVoltageAlert');
                const maxVoltageAlertCheckbox = document.getElementById('maxVoltageAlert');
                const selectedVoltageRadio = document.querySelector('input[name="voltmeterVoltage"]:checked');

                if (!voltmeterNameInput || !voltageMinInput || !voltageMaxInput || !selectedVoltageRadio) {
                    console.error("❌ Error: One or more required voltmeter inputs are missing.");
                    return;
                }

                const voltmeterName = voltmeterNameInput.value.trim();
                const systemVoltage = parseInt(selectedVoltageRadio.value);
                const minVoltage = parseFloat(voltageMinInput.value);
                const maxVoltage = parseFloat(voltageMaxInput.value);
                const minVoltageAlert = minVoltageAlertCheckbox.checked;
                const maxVoltageAlert = maxVoltageAlertCheckbox.checked;

                let voltIndex = currentVoltmeterIndex;
                if (voltmeterConfigMode === "1S2V" && currentVoltmeterIndex > 0) {
                    voltIndex -= 1;
                }

                voltmetersData[voltIndex] = {
                    id: voltmetersData[voltIndex]?.id || (voltIndex + 1),
                    voltmeterName,
                    systemVoltage,
                    minVoltage,
                    maxVoltage,
                    value: voltmetersData[voltIndex]?.value || 0,
                    alerts: {
                        minVoltage: minVoltageAlert,
                        maxVoltage: maxVoltageAlert
                    }
                };

                updatedData = {
                    command: 'updateVoltmeterConfiguration',
                    voltmeterData: voltmetersData[voltIndex],
                    configMode: voltmeterConfigMode
                };

                console.log(`✅ Voltmeter updated:`, voltmetersData[voltIndex]);
            }

            // Now send the shunt/voltmeter update
            return sendDataToServer(updatedData, true)
                .then(() => {
                    console.log(`✅ ${updatedData.command} successfully sent.`);
                });
        })
        .then(() => {
            console.log("✅ All config updates acknowledged by server.");

            configWizardActive = false;
            setVoltmeterBtn.disabled = true;

            closeVoltmeterSetupPopup();
            closeSettingsPopup();

            frameRightRendered = false;
            setupFrameRight(true);
        })
        .catch((err) => {
            console.error("❌ Error during config update sequence:", err);
        });
}


function clearVoltmeterForm() {
    document.getElementById("voltmeterName").value = "";
    document.querySelector('input[name="voltmeterType"]').checked = false;
}

function openVoltageSetupWizard() {
    configWizardActive = true;
  console.log("🔧 Opening Voltage Setup Wizard...");

  // Show the popup
  document.getElementById("configModePop").style.display = "flex";

  // Determine the current mode (fallback to 4V)
  const currentMode = voltmeterConfigMode || "4V";
  const radioButton = document.querySelector(`input[name="configMode"][value="${currentMode}"]`);

  if (radioButton) {
      radioButton.checked = true;
  }

  // Force update the image when opening the popup
  updateConfigModeImage();
}

function closeConfigModePopup(setInactive = true) {
    if (setInactive) configWizardActive = false;
    document.getElementById("configModePop").style.display = "none";
}

function updateConfigModeImage() {
    const configModeImageMap = {
        "4V": "configMode_4V.jpg",
        "2V1S": "configMode_2V1S.jpg",
        "1S2V": "configMode_1S2V.jpg",
        "2S": "configMode_2S.jpg"
    };

    const selectedMode = document.querySelector('input[name="configMode"]:checked')?.value;
    const imgEl = document.getElementById("configModeImage");

    if (selectedMode && configModeImageMap[selectedMode] && imgEl) {
        const imagePath = "/images/" + configModeImageMap[selectedMode];
        imgEl.dataset.src = imagePath; // Update the data-src too
        imgEl.src = imagePath;
        console.log(`🔄 Updated config image to: ${imagePath}`);
    } else {
        console.error("❌ No valid image found for mode:", selectedMode);
    }
}


function confirmConfigModeSelection() {
    const selectedMode = document.querySelector('input[name="configMode"]:checked')?.value;
    if (!selectedMode) {
        alert("Please select a configuration mode.");
        return;
    }

    console.log("✅ Configuration mode selected:", selectedMode);

    // Store the selection
    voltmeterConfigMode = selectedMode;

    // Close popup and move to the next step
    closeConfigModePopup();
}

function confirmConfigModeSelection() {
    const selectedMode = document.querySelector('input[name="configMode"]:checked')?.value;
    if (!selectedMode) {
        alert("Please select a configuration mode.");
        return;
    }

    console.log("✅ Configuration mode selected:", selectedMode);
    voltmeterConfigMode = selectedMode; // Save selection

    // Open the confirmation popup
    openConfigConfirmPopup(selectedMode);
}

function openConfigConfirmPopup(mode) {
    console.log("🔧 Opening Wiring Confirmation Popup for mode:", mode);

    const wiringImageMap = {
        "4V": "wiringMode_4V.jpg",
        "2V1S": "wiringMode_2V1S.jpg",
        "1S2V": "wiringMode_1S2V.jpg",
        "2S": "wiringMode_2S.jpg"
    };

    const wiringMessageMap = {
        "4V": "All four inputs are configured as individual voltmeters.",
        "2V1S": "Connect <strong>Voltmeters</strong> to Inputs 1 & 2, and the <strong>Shunt</strong> to Inputs 3 & 4.",
        "1S2V": "Connect the <strong>Shunt</strong> to Inputs 1 & 2, and <strong>Voltmeters</strong> to Inputs 3 & 4.",
        "2S": "Connect <strong>Shunt 1</strong> to Inputs 1 & 2, and <strong>Shunt 2</strong> to Inputs 3 & 4."
    };

    const imageFile = wiringImageMap[mode];
    const messageText = wiringMessageMap[mode];
    const imgEl = document.getElementById("configConfirmImage");
    const msgEl = document.getElementById("configConfirmMessage");

    if (!imageFile || !imgEl || !msgEl) {
        console.error("❌ Invalid mode or missing DOM elements!");
        return;
    }

    const imagePath = "/images/" + imageFile;
    console.log("🖼 Attempting to load wiring image:", imagePath);

    imgEl.onerror = () => {
        console.error("❌ Failed to load:", imagePath);
        imgEl.alt = "⚠️ Image failed to load";
        imgEl.style.border = "2px solid red";
    };

    imgEl.onload = () => {
        console.log("✅ Wiring image loaded:", imagePath);
        imgEl.style.border = "none";
    };

    // Lazy-load the image
    imgEl.dataset.src = imagePath;
    imgEl.src = imagePath;

    msgEl.innerHTML = messageText;

    // Show the popup
    document.getElementById("configConfirmPop").style.display = "flex";
}

function finalizeConfigMode() {
    console.log("✅ Configuration confirmed:", voltmeterConfigMode);

    // Close popups
    closeConfigConfirmPopup(false);
    closeConfigModePopup(false);

    // Reset arrays
    voltmetersData = [];
    shuntsData = [];

    // Generate placeholders based on mode
    switch (voltmeterConfigMode) {
        case "4V":
            for (let i = 0; i < 4; i++) {
                voltmetersData.push({
                    id: i + 1,
                    voltmeterName: `Voltmeter ${i + 1}`,
                    systemVoltage: 12,
                    minVoltage: 0,
                    maxVoltage: 15,
                    value: 0,
                    alerts: { minVoltage: false, maxVoltage: false }
                });
            }
            break;

        case "2V1S":
            // Voltmeters 1 & 2
            for (let i = 0; i < 2; i++) {
                voltmetersData.push({
                    id: i + 1,
                    voltmeterName: `Voltmeter ${i + 1}`,
                    systemVoltage: 12,
                    minVoltage: 0,
                    maxVoltage: 15,
                    value: 0,
                    alerts: { minVoltage: false, maxVoltage: false }
                });
            }

            // Shunt 1
            shuntsData.push({
                id: 1,
                shuntName: "Shunt 1",
                smartShunt: false,
                shuntMV: 50,
                systemVoltage: 12,
                autoSOC: false,
                minSOC: 50,
                alerts: { minSOC: false }
            });
            break;

        case "1S2V":
            // Shunt 1
            shuntsData.push({
                id: 1,
                shuntName: "Shunt 1",
                smartShunt: false,
                shuntMV: 50,
                systemVoltage: 12,
                autoSOC: false,
                minSOC: 50,
                alerts: { minSOC: false }
            });

            // Voltmeters 3 & 4
            for (let i = 0; i < 2; i++) {
                voltmetersData.push({
                    id: i + 1,
                    voltmeterName: `Voltmeter ${i + 1}`,
                    systemVoltage: 12,
                    minVoltage: 0,
                    maxVoltage: 15,
                    value: 0,
                    alerts: { minVoltage: false, maxVoltage: false }
                });
            }
            break;

        case "2S":
            // Shunt 1 & 2
            for (let i = 0; i < 2; i++) {
                shuntsData.push({
                    id: i + 1,
                    shuntName: `Shunt ${i + 1}`,
                    smartShunt: true,
                    shuntMV: 50,
                    systemVoltage: 12,
                    autoSOC: true,
                    minSOC: 50,
                    alerts: { minSOC: false }
                });
            }
            break;

        default:
            console.warn("⚠️ Unknown voltmeterConfigMode:", voltmeterConfigMode);
    }

    // ✅ Reset index and open voltmeter/shunt popup
    currentVoltmeterIndex = 0;
    generateVoltmeterPopup();
}

function closeConfigConfirmPopup(setInactive = true) {
    if (setInactive) configWizardActive = false;
    document.getElementById("configConfirmPop").style.display = "none";
}

function closeVoltmeterSetupPopup() {
    document.getElementById('voltmeterSetupPop').style.display = 'none';
}

const fs = require("fs");
const path = require("path");
const { exec, execSync } = require("child_process");

const templateFiles = ["platformio.ini", "src", "include", "data", "scripts"];
const moduleName = process.argv[2] || "NewSmartModule";
const targetDir = path.join(__dirname, "..", "..", moduleName);
const smartCoreRepo = "https://github.com/SmartBoat2024/SmartCore";
const pioPath = path.join(process.env.USERPROFILE, ".platformio", "penv", "Scripts", "platformio.exe");

// Create project folder
if (!fs.existsSync(targetDir)) fs.mkdirSync(targetDir);

// Copy all relevant files
templateFiles.forEach(file => {
  const src = path.join(__dirname, "..", file);
  const dest = path.join(targetDir, file === "data" ? "data" : path.basename(file));
  if (fs.existsSync(src)) fs.cpSync(src, dest, { recursive: true });
});

// Patch platformio.ini
const pioIniPath = path.join(targetDir, "platformio.ini");
if (fs.existsSync(pioIniPath)) {
  let content = fs.readFileSync(pioIniPath, "utf8");
  content = content.replace(/^\s*lib_extra_dirs\s*=.*$/gm, "");
  if (content.includes("lib_deps")) {
    content = content.replace(/lib_deps\s*=\s*(.*)/, (match, existing) => {
      return existing.includes(smartCoreRepo) ? match :
        `lib_deps = ${smartCoreRepo}\n    ${existing.trim().replace(/^/gm, "    ")}`;
    });
  } else {
    content = content.replace(/\[env:.*?\]/, match => `${match}\nlib_deps = ${smartCoreRepo}`);
  }
  fs.writeFileSync(pioIniPath, content);
  console.log("🔄 Cleaned lib_extra_dirs and added SmartCore GitHub lib_deps");
}

console.log(`✅ Module ${moduleName} created at ${targetDir}`);

// Find connected device
let comPort = "";
try {
  const result = execSync(`"${pioPath}" device list`).toString();
  const match = result.match(/(COM\d+)/i); // First COM port
  if (match) comPort = match[1];
} catch (e) {
  console.warn("⚠️ Unable to list COM ports.");
}

// Open in VS Code
const readline = require("readline");

// Open the module in VS Code
exec(`code "${targetDir}"`, () => {
  if (!comPort) {
    console.warn("🔌 No COM port detected — skipping LittleFS erase/upload.");
    return;
  }

  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

  rl.question("🧯 Erase and upload LittleFS data folder now? (Y/n): ", (answer) => {
    rl.close();

    const confirmed = answer.trim().toLowerCase() !== "n";
    if (!confirmed) {
      console.log("🚫 Skipped LittleFS upload.");
      return;
    }

    console.log(`🧨 Device detected on ${comPort}. Starting LittleFS erase/upload...`);

    const eraseCmd = `"${pioPath}" run --target erasefs`;
    const uploadCmd = `"${pioPath}" run --target uploadfs`;
    const opts = { cwd: targetDir, env: { ...process.env, PLATFORMIO_UPLOAD_PORT: comPort } };

    exec(eraseCmd, opts, (eraseErr, eraseStdout) => {
      if (eraseErr) {
        console.error("❌ Failed to erase LittleFS:", eraseErr.message);
        return;
      }

      console.log("🧼 LittleFS erased. Uploading new assets...");
      exec(uploadCmd, opts, (uploadErr, uploadStdout) => {
        if (uploadErr) {
          console.error("❌ Failed to upload LittleFS:", uploadErr.message);
        } else {
          console.log("📂 LittleFS assets uploaded successfully.");
          console.log(uploadStdout);
        }
      });
    });
  });
});




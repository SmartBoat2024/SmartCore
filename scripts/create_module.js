const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const templateFiles = ["platformio.ini", "src", "include", "data", "scripts"];
const moduleName = process.argv[2] || "NewSmartModule";
const targetDir = path.join(__dirname, "..", "..", moduleName);
const smartCoreRepo = "https://github.com/SmartBoat2024/SmartCore";

// Create target directory
if (!fs.existsSync(targetDir)) {
  fs.mkdirSync(targetDir);
}

// Copy files
templateFiles.forEach(file => {
  const src = path.join(__dirname, "..", file);
  const dest = file === "data"
    ? path.join(targetDir, "data")
    : path.join(targetDir, path.basename(file));

  if (fs.existsSync(src)) {
    fs.cpSync(src, dest, { recursive: true });
  }
});

// Update platformio.ini
const pioIniPath = path.join(targetDir, "platformio.ini");
if (fs.existsSync(pioIniPath)) {
  let content = fs.readFileSync(pioIniPath, "utf8");
  content = content.replace(/^\s*lib_extra_dirs\s*=.*$/gm, "");

  if (content.includes("lib_deps")) {
    content = content.replace(/lib_deps\s*=\s*(.*)/, (match, existing) => {
      if (existing.includes(smartCoreRepo)) return match;
      return `lib_deps = ${smartCoreRepo}\n    ${existing.trim().replace(/^/gm, "    ")}`;
    });
  } else {
    content = content.replace(/\[env:.*?\]/, match => `${match}\nlib_deps = ${smartCoreRepo}`);
  }

  fs.writeFileSync(pioIniPath, content);
  console.log("🔄 Cleaned lib_extra_dirs and added SmartCore GitHub lib_deps");
}

console.log(`✅ Module ${moduleName} created at ${targetDir}`);

// Try to upload LittleFS
const pioPath = path.join(process.env.USERPROFILE, ".platformio", "penv", "Scripts", "platformio.exe");
if (!fs.existsSync(pioPath)) {
  console.warn("⚠️ PlatformIO not found — skipping LittleFS upload.");
  return;
}

exec(`code "${targetDir}"`, () => {
  const pioCmd = `"${pioPath}" run --target uploadfs`;
  exec(pioCmd, { cwd: targetDir }, (err, stdout, stderr) => {
    if (err) {
      console.error("❌ Failed to format/upload LittleFS:", err.message);
    } else {
      console.log("🧹 LittleFS formatted and 📂 new assets uploaded.");
      console.log(stdout);
    }
  });
});


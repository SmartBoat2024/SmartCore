const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const templateFiles = ["platformio.ini", "src", "include", "scripts"];
const moduleName = process.argv[2] || "NewSmartModule";
const targetDir = path.join(__dirname, "..", "..", moduleName);

// Single repo URL containing both SmartCore and ESPAsync_WiFiManager via export
const smartCoreRepo = "https://github.com/SmartBoat2024/SmartCore";

// Create target directory
if (!fs.existsSync(targetDir)) {
  fs.mkdirSync(targetDir);
}

// Copy files
templateFiles.forEach(file => {
  const src = path.join(__dirname, "..", file);
  const dest = path.join(targetDir, path.basename(file));
  if (fs.existsSync(src)) {
    fs.cpSync(src, dest, { recursive: true });
  }
});

// Update platformio.ini
const pioIniPath = path.join(targetDir, "platformio.ini");
if (fs.existsSync(pioIniPath)) {
  let content = fs.readFileSync(pioIniPath, "utf8");

  // Remove any dev-specific lib_extra_dirs line
  content = content.replace(/^\s*lib_extra_dirs\s*=.*$/gm, "");

  // Inject the SmartCore GitHub repo as lib_deps (clean and top-level)
  if (content.includes("lib_deps")) {
    content = content.replace(
      /lib_deps\s*=\s*(.*)/,
      (match, existing) => {
        if (existing.includes(smartCoreRepo)) return match;
        return `lib_deps = ${smartCoreRepo}\n    ${existing.trim().replace(/^/gm, '    ')}`;
      }
    );
  } else {
    content = content.replace(
      /\[env:.*?\]/,
      match => `${match}\nlib_deps = ${smartCoreRepo}`
    );
  }

  fs.writeFileSync(pioIniPath, content);
  console.log("🔄 Cleaned lib_extra_dirs and added SmartCore GitHub lib_deps");
}

console.log(`✅ Module ${moduleName} created at ${targetDir}`);

// Open the new module in VS Code
exec(`code "${targetDir}"`);

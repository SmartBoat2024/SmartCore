const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const templateFiles = ["platformio.ini", "src", "include", "scripts"];
const moduleName = process.argv[2] || "NewSmartModule";
const targetDir = path.join(__dirname, "..", "..", moduleName);

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

  // Remove lib_extra_dirs entirely
  content = content.replace(/^\s*lib_extra_dirs\s*=.*$/gm, "");

  // Inject SmartCore lib_deps if not already included
  const smartCoreLib = "https://github.com/SmartBoat2024/SmartCore";
  if (!content.includes(smartCoreLib)) {
    content = content.replace(
      /\[env:.*?\]/,
      match => `${match}\nlib_deps = ${smartCoreLib}`
    );
  }

  fs.writeFileSync(pioIniPath, content);
  console.log("🔄 Cleaned lib_extra_dirs and added SmartCore to lib_deps");
}

console.log(`✅ Module ${moduleName} created at ${targetDir}`);

// Open the new module in VS Code
exec(`code "${targetDir}"`);


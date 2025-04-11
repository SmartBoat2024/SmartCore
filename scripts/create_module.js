const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const templateFiles = ["platformio.ini", "src", "include", "scripts"]; // main.cpp is now in src

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

// 🔁 Replace dev-only line in platformio.ini
const pioIniPath = path.join(targetDir, "platformio.ini");
if (fs.existsSync(pioIniPath)) {
  let content = fs.readFileSync(pioIniPath, "utf8");

  content = content.replace(
    /^\s*lib_extra_dirs\s*=.*$/m,
    'lib_deps = https://github.com/SmartBoat2024/SmartCore'
  );

  fs.writeFileSync(pioIniPath, content);
  console.log("🔄 Swapped lib_extra_dirs with SmartCore GitHub lib_deps");
}

console.log(`✅ Module ${moduleName} created at ${targetDir}`);

// Open in VS Code
exec(`code "${targetDir}"`);


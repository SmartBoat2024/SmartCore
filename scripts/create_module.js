// create_module.js
const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const templateFiles = ["platformio.ini", "src", "include", "main.cpp"]; // Add or adjust

const moduleName = process.argv[2] || "NewSmartModule";
const targetDir = path.join(__dirname, "..", "..", moduleName); // Adjust as needed

if (!fs.existsSync(targetDir)) {
  fs.mkdirSync(targetDir);
}

templateFiles.forEach(file => {
  const src = path.join(__dirname, "..", file);
  const dest = path.join(targetDir, path.basename(file));
  if (fs.existsSync(src)) {
    fs.cpSync(src, dest, { recursive: true });
  }
});

console.log(`✅ Module ${moduleName} created at ${targetDir}`);

// Auto-open VS Code in that folder
exec(`code "${targetDir}"`);

const status = document.querySelector("#installer-status");
const installer = document.querySelector("esp-web-install-button");

async function initialize() {
  if (!window.isSecureContext) {
    status.textContent = "USB installation requires HTTPS. Open the published installer, or use localhost for a local preview.";
    return;
  }
  if (!("serial" in navigator)) {
    status.textContent = "This browser does not support Web Serial. Open this page in desktop Chrome or Edge, or use the manual USB instructions below.";
    return;
  }

  let timeout;
  try {
    // Pin the library so releases do not silently change their flashing behavior.
    await Promise.race([
      import("https://unpkg.com/esp-web-tools@10.1.0/dist/web/install-button.js?module"),
      new Promise((_, reject) => {
        timeout = setTimeout(() => reject(new Error("Installer download timed out")), 15000);
      }),
    ]);
    installer.hidden = false;
    status.textContent = "Ready to connect over USB.";
  } catch {
    status.textContent = "The USB installer could not load. Check your internet connection and reload this page, or use the manual USB instructions below.";
  } finally {
    clearTimeout(timeout);
  }
}

initialize();

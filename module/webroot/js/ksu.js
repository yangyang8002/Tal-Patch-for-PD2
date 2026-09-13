/**
 * KernelSU WebUI 桥接层
 * 封装 window.ksu.exec（JSInterface，回调名为全局函数字符串）为 Promise。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
(function (global) {
  'use strict';

  let seq = 0;

  function ksuAvailable() {
    return typeof global.ksu !== 'undefined' && typeof global.ksu.exec === 'function';
  }

  /**
   * 以 root 执行 shell 命令。
   * @param {string} cmd
   * @returns {Promise<{errno:number, stdout:string, stderr:string}>}
   */
  function exec(cmd, timeoutMs) {
    return new Promise((resolve, reject) => {
      if (!ksuAvailable()) {
        reject(new Error('KernelSU WebUI API 不可用，请在 KernelSU 管理器中打开本页'));
        return;
      }
      const cbName = '__talpatch_cb_' + (++seq) + '_' + Date.now();
      const timer = setTimeout(() => {
        delete global[cbName];
        reject(new Error('命令执行超时'));
      }, timeoutMs || 15000);
      global[cbName] = function (errno, stdout, stderr) {
        clearTimeout(timer);
        delete global[cbName];
        resolve({ errno: errno | 0, stdout: String(stdout || ''), stderr: String(stderr || '') });
      };
      try {
        global.ksu.exec(cmd, '{}', cbName);
      } catch (e) {
        clearTimeout(timer);
        delete global[cbName];
        reject(e);
      }
    });
  }

  /** 读取文本文件（root）。 */
  async function readFile(path) {
    const r = await exec('cat "' + path + '" 2>/dev/null');
    return r.errno === 0 ? r.stdout : '';
  }

  /** 写入文本文件（base64 防注入）。 */
  async function writeFile(path, content) {
    const b64 = btoa(unescape(encodeURIComponent(content)));
    const r = await exec(
      'echo "' + b64 + '" | base64 -d > "' + path + '" && chmod 0644 "' + path + '"');
    return r.errno === 0;
  }

  /** 系统 toast（可用时）。 */
  function toast(msg) {
    if (typeof global.ksu !== 'undefined' && typeof global.ksu.toast === 'function') {
      try { global.ksu.toast(msg); return; } catch (e) { /* ignore */ }
    }
    const el = document.getElementById('toast');
    if (el) {
      el.textContent = msg;
      el.classList.add('show');
      clearTimeout(el.__timer);
      el.__timer = setTimeout(() => el.classList.remove('show'), 2200);
    }
  }

  global.ksuBridge = { exec, readFile, writeFile, toast, ksuAvailable };
})(window);

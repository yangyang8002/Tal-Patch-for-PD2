# -*- coding: utf-8 -*-
# 为 app.js 添加「消息通知（应用粒度）」逻辑
import io

p = 'module/webroot/js/app.js'
s = io.open(p, encoding='utf-8').read()
NL = chr(10)

# 1) DEFAULTS 增加两个键
old = "    scope_extra: ''," + NL + "    ui_theme: 'miuix'" + NL + "  };"
new = ("    scope_extra: ''," + NL + "    ui_theme: 'miuix'," + NL
       + "    notify_all_enabled: true," + NL + "    notify_app_overrides: {}" + NL + "  };")
assert old in s, 'DEFAULTS anchor'
s = s.replace(old, new)

# 2) loadConfig 归一化 overrides + 初始化本节
old = "    $('statusText').textContent = ksuBridge.ksuAvailable()"
new = ("        if (typeof config.notify_app_overrides === 'string') {\n"
       "          try { config.notify_app_overrides = JSON.parse(config.notify_app_overrides || '{}'); }\n"
       "          catch (e) { config.notify_app_overrides = {}; }\n"
       "        }\n"
       "        initNotifySection();\n"
       "        $('statusText').textContent = ksuBridge.ksuAvailable()")
assert old in s, 'loadConfig anchor'
s = s.replace(old, new, 1)

# 3) 应用列表逻辑块（插入「维护操作」注释前）
anchor = "  // ---------------- 维护操作 ----------------"
block = r"""    // ---------------- 消息通知（应用粒度） ----------------
    var appList = [];        // [{pkg: string, system: boolean}]
    var origEffective = {};  // 扫描时的生效状态基线
    var changedApps = {};    // 与基线不同的包名 -> 当前生效状态
    var MAX_LIST_ROWS = 300;

    function getOverrides() {
      var ov = config.notify_app_overrides;
      if (typeof ov === 'string') {
        try { ov = JSON.parse(ov || '{}'); } catch (e) { ov = {}; }
        config.notify_app_overrides = ov;
      }
      if (!ov || typeof ov !== 'object') {
        ov = {};
        config.notify_app_overrides = ov;
      }
      return ov;
    }

    // 生效状态 = 覆盖表[pkg] ?? 全局默认
    function isNotifyOn(pkg) {
      var ov = getOverrides();
      if (Object.prototype.hasOwnProperty.call(ov, pkg)) return !!ov[pkg];
      return !!config.notify_all_enabled;
    }

    function initNotifySection() {
      syncNotifyAllSwitch();
      updateScopeBar();
    }

    function syncNotifyAllSwitch() {
      var sw = $('notifyAllSwitch');
      var on = !!config.notify_all_enabled;
      sw.classList.toggle('on', on);
      sw.setAttribute('aria-checked', String(on));
    }

    // 重新计算变更集（master 开关变化后全体重算）
    function recomputeChanged() {
      changedApps = {};
      appList.forEach(function (app) {
        var cur = isNotifyOn(app.pkg);
        if (origEffective[app.pkg] !== undefined && origEffective[app.pkg] !== cur) {
          changedApps[app.pkg] = cur;
        }
      });
    }

    function updateScopeBar() {
      var n = Object.keys(changedApps).length;
      $('scopeBarText').textContent = '已变更 ' + n + ' 个应用的作用域';
      $('scopeBar').classList.toggle('show', n > 0);
    }

    function parsePackageLines(stdout) {
      var out = [];
      (stdout || '').split('\n').forEach(function (line) {
        line = line.trim();
        if (line.indexOf('package:') === 0) {
          var pkg = line.substring(8).trim();
          if (pkg) out.push(pkg);
        }
      });
      return out;
    }

    async function scanApps() {
      $('appListInfo').textContent = '正在扫描…';
      $('btnScanApps').disabled = true;
      try {
        var rUser = await ksuBridge.exec('pm list packages -3');
        var rSys = await ksuBridge.exec('pm list packages -s');
        var userPkgs = parsePackageLines(rUser.stdout);
        var sysPkgs = parsePackageLines(rSys.stdout);
        var seen = {};
        appList = [];
        userPkgs.forEach(function (p) { if (!seen[p]) { seen[p] = 1; appList.push({ pkg: p, system: false }); } });
        sysPkgs.forEach(function (p) { if (!seen[p]) { seen[p] = 1; appList.push({ pkg: p, system: true }); } });
        appList.sort(function (a, b) { return a.pkg < b.pkg ? -1 : 1; });
        // 记录基线（用于"重启变更作用域"）
        origEffective = {};
        appList.forEach(function (app) { origEffective[app.pkg] = isNotifyOn(app.pkg); });
        recomputeChanged();
        renderAppList();
        updateScopeBar();
        $('appListInfo').textContent =
          '共 ' + appList.length + ' 个应用（用户 ' + userPkgs.length + ' / 系统 ' + sysPkgs.length + '）';
        ksuBridge.toast('扫描完成：' + appList.length + ' 个应用');
      } catch (e) {
        $('appListInfo').textContent = '扫描失败：' + e.message;
      } finally {
        $('btnScanApps').disabled = false;
      }
    }

    function renderAppList() {
      var box = $('appList');
      box.innerHTML = '';
      if (!appList.length) return;
      var q = ($('appSearchInput').value || '').trim().toLowerCase();
      var shown = 0;
      var frag = document.createDocumentFragment();
      for (var i = 0; i < appList.length && shown < MAX_LIST_ROWS; i++) {
        var app = appList[i];
        if (q && app.pkg.toLowerCase().indexOf(q) < 0) continue;
        shown++;
        var row = document.createElement('div');
        row.className = 'row app-row';
        var textWrap = document.createElement('div');
        textWrap.className = 'row-text';
        var title = document.createElement('div');
        title.className = 'row-title app-pkg';
        title.textContent = app.pkg;
        textWrap.appendChild(title);
        var badge = document.createElement('span');
        badge.className = 'badge' + (app.system ? ' badge-sys' : '');
        badge.textContent = app.system ? '系统' : '用户';
        textWrap.appendChild(badge);
        row.appendChild(textWrap);
        var sw = document.createElement('button');
        sw.className = 'switch';
        sw.setAttribute('role', 'switch');
        var on = isNotifyOn(app.pkg);
        sw.classList.toggle('on', on);
        sw.setAttribute('aria-checked', String(on));
        sw.innerHTML = '<span class="thumb"></span>';
        (function (pkg, btn) {
          btn.onclick = function () { toggleApp(pkg); };
        })(app.pkg, sw);
        row.appendChild(sw);
        frag.appendChild(row);
      }
      box.appendChild(frag);
      if (shown >= MAX_LIST_ROWS) {
        var more = document.createElement('div');
        more.className = 'app-list-info';
        more.textContent = '仅显示前 ' + MAX_LIST_ROWS + ' 条，请用搜索缩小范围';
        box.appendChild(more);
      } else if (shown === 0) {
        var none = document.createElement('div');
        none.className = 'app-list-info';
        none.textContent = '无匹配应用';
        box.appendChild(none);
      }
    }

    function toggleApp(pkg) {
      var next = !isNotifyOn(pkg);
      var ov = getOverrides();
      if (next === !!config.notify_all_enabled) {
        delete ov[pkg];   // 与默认值一致则移除覆盖，保持表干净
      } else {
        ov[pkg] = next;
      }
      if (origEffective[pkg] !== undefined && origEffective[pkg] !== next) {
        changedApps[pkg] = next;
      } else {
        delete changedApps[pkg];
      }
      saveConfig(true);
      renderAppList();
      updateScopeBar();
    }

    async function restartChangedApps() {
      var pkgs = Object.keys(changedApps);
      if (!pkgs.length) return;
      var cmd = pkgs.map(function (p) { return 'am force-stop ' + p; }).join('; ');
      var r = await ksuBridge.exec(cmd);
      if (r.errno === 0) {
        ksuBridge.toast('已重启 ' + pkgs.length + ' 个应用（前台应用重新打开后生效）');
        pkgs.forEach(function (p) { origEffective[p] = changedApps[p]; });
        changedApps = {};
        updateScopeBar();
      } else {
        ksuBridge.toast('部分应用重启失败：' + (r.stderr || r.errno));
      }
    }

""" + anchor
assert anchor in s, '维护操作 anchor'
s = s.replace(anchor, block, 1)

# 4) 绑定事件（插在 themeGroup 监听之前）
old4 = "    $('themeGroup').addEventListener('click', function (e) {"
new4 = """    $('notifyAllSwitch').onclick = function () {
        config.notify_all_enabled = !config.notify_all_enabled;
        syncNotifyAllSwitch();
        recomputeChanged();
        saveConfig();
        renderAppList();
        updateScopeBar();
      };
      $('btnScanApps').onclick = scanApps;
      $('appSearchInput').oninput = renderAppList;
      $('btnRestartChanged').onclick = restartChangedApps;
      $('btnDismissScopeBar').onclick = function () {
        changedApps = {};
        updateScopeBar();
      };
      $('themeGroup').addEventListener('click', function (e) {"""
assert old4 in s, 'themeGroup anchor'
s = s.replace(old4, new4, 1)

io.open(p, 'w', encoding='utf-8').write(s)
print('app.js patched, lines:', len(s.splitlines()))

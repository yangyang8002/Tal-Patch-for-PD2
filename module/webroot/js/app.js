/**
 * TAL-Patch WebUI 主逻辑：配置读写、双主题切换（MIUIX / Material）、维护操作。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
(function () {
  'use strict';

  var CONFIG_PATH = '/data/adb/modules/tal_patch/config/config.json';
  var MIRROR_PATH = '/sdcard/TAL-Patch/config.json';
  var LOG_PATH = '/sdcard/TAL-Patch-Log/tal_patch.log';

  var DEFAULTS = {
    unlock_install: true,
    block_usercenter_detect: true,
    restore_notification: true,
    block_default_wallpaper: false,
    block_default_launcher: false,
    custom_usage_enabled: false,
    custom_study_minutes: '120',
    study_once_per_day: true,
    custom_app_usage: '',
    block_study_report: true,
    block_minor_control: true,
    block_root_logs: true,
    scope_extra: '',
    ui_theme: 'miuix',
    notify_all_enabled: true,
    notify_app_overrides: {}
  };

  var TAL_PACKAGES = [
    'com.tal.pad.studyservice', 'com.tal.dataupload', 'com.tal.pad.backdoor',
    'com.tal.backdoor', 'com.tal.pad.onlineclass', 'com.tal.pad.usercenter',
    'com.tal.pad.minor_protect', 'com.tal.pad.znxxservice'
  ];

  // 配置项 schema：type: switch | text | textarea
  var SCHEMA = [
    { section: '安装与检测', page: 'install' },
    { key: 'unlock_install', type: 'switch', title: '解除学习机系统安装限制',
      desc: '放行任意应用安装：绕过安装白名单、未知来源与 V 型设备限制' },
    { key: 'block_usercenter_detect', type: 'switch', title: '阻止用户中心检测环境并上报日志',
      desc: '屏蔽用户中心的 root / 框架 / 模拟器 / 调试等环境检测与设备上报' },
    { section: '系统行为', page: 'system' },
    { key: 'restore_notification', type: 'switch', title: '恢复通知消息内容',
      desc: '还原被隐藏的通知标题、正文、进度条和操作按钮' },
    { key: 'block_default_wallpaper', type: 'switch', title: '阻止系统恢复默认壁纸',
      desc: '防止系统在定制或开机时把壁纸重置成默认' },
    { key: 'block_default_launcher', type: 'switch', title: '阻止系统恢复默认桌面',
      desc: '防止回桌面时把默认桌面改回 TAL 自带桌面' },
    { section: '学习与应用统计', page: 'stats' },
    { key: 'custom_usage_enabled', type: 'switch', title: '自定义学习时长与应用使用',
      desc: '开启后，按下面填写的时长代替真实统计' },
    { key: 'custom_study_minutes', type: 'text', title: '每日学习时长（分钟）',
      desc: '每天的学习时长显示为这个数字', hint: '例如：120', input: 'number' },
    { key: 'study_once_per_day', type: 'switch', title: '每日仅伪装一次学习时长',
      desc: '开启：只有当天第一节课补发伪装值；关闭：每节课都补发' },
    { key: 'custom_app_usage', type: 'textarea', title: '应用使用时长',
      desc: '每行一个：包名:分钟（空格或全角冒号也可以）',
      hint: 'com.microsoft.emmx 6\ncom.tencent.mm:30' },
    { key: 'block_study_report', type: 'switch', title: '伪装学习时长上报（含未成年人保护）',
      desc: '拦截真实学习时长（xpad_study_duration）上报，替换为自定义学习时长' },
    { key: 'block_minor_control', type: 'switch', title: '禁用未成年人保护精细化管控',
      desc: '所有应用均允许启动，不再下发或执行禁止使用指令' },
    { key: 'block_root_logs', type: 'switch', title: '限制上传 Root 痕迹日志',
      desc: '让 backdoor 上报的日志看起来像未 Root 的正常设备' },
    { section: '高级', page: 'advanced' },
    { key: 'scope_extra', type: 'text', title: '附加作用域',
      desc: '附加注入的应用包名，逗号分隔（这些应用只恢复通知，需重启目标应用）',
      hint: '例如：com.tencent.mm, com.microsoft.emmx' }
  ];

  var config = {};
  for (var k in DEFAULTS) config[k] = DEFAULTS[k];

  function $(id) { return document.getElementById(id); }

  // ---------------- 主题 ----------------
  function currentTheme() {
    return localStorage.getItem('tal_patch_ui_theme') || config.ui_theme || 'miuix';
  }

  function applyTheme(theme, persist) {
    if (theme !== 'material') theme = 'miuix';
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('tal_patch_ui_theme', theme);
    var btns = document.querySelectorAll('.seg-btn');
    for (var i = 0; i < btns.length; i++) {
      btns[i].classList.toggle('active', btns[i].getAttribute('data-value') === theme);
    }
    if (persist && config.ui_theme !== theme) {
      config.ui_theme = theme;
      saveConfig(true);
    }
  }

  // ---------------- 渲染 ----------------
  function render() {
    var container = $('configContainer');
    container.innerHTML = '';
    var card = null;
    var page = null;
    var pages = {};
    SCHEMA.forEach(function (item) {
      if (item.section) {
        page = document.createElement('div');
        page.className = 'tab-page';
        page.setAttribute('data-page', item.page);
        var h = document.createElement('div');
        h.className = 'section-title';
        h.textContent = item.section;
        page.appendChild(h);
        card = document.createElement('div');
        card.className = 'card';
        page.appendChild(card);
        container.appendChild(page);
        pages[item.page] = page;
        return;
      }
      if (!card) return;
      var row = document.createElement('div');
      row.className = 'row';
      var textWrap = document.createElement('div');
      textWrap.className = 'row-text';
      var title = document.createElement('div');
      title.className = 'row-title';
      title.textContent = item.title;
      var desc = document.createElement('div');
      desc.className = 'row-desc';
      desc.textContent = item.desc;
      textWrap.appendChild(title);
      textWrap.appendChild(desc);
      row.appendChild(textWrap);

      if (item.type === 'switch') {
        var sw = document.createElement('button');
        sw.className = 'switch';
        sw.setAttribute('role', 'switch');
        sw.setAttribute('aria-checked', String(!!config[item.key]));
        sw.classList.toggle('on', !!config[item.key]);
        sw.innerHTML = '<span class="thumb"></span>';
        sw.onclick = function () {
          config[item.key] = !config[item.key];
          sw.classList.toggle('on', config[item.key]);
          sw.setAttribute('aria-checked', String(config[item.key]));
          saveConfig();
        };
        row.appendChild(sw);
      } else {
        row.classList.add('row-column');
        var input = document.createElement(item.type === 'textarea' ? 'textarea' : 'input');
        input.className = 'input';
        input.placeholder = (item.hint || '').replace(/\\n/g, '\n');
        if (item.input) input.setAttribute('inputmode', item.input);
        if (item.type === 'textarea') input.rows = 4;
        input.value = config[item.key] == null ? '' : String(config[item.key]);
        input.onchange = function () {
          config[item.key] = input.value;
          saveConfig();
        };
        row.appendChild(input);
      }
      card.appendChild(row);
    });
    applyTab(currentTab(), false);
  }

  // ---------------- Tab 分组栏 ----------------
  function currentTab() {
    return localStorage.getItem('tal_patch_tab') || 'install';
  }

  function applyTab(pageId, persist) {
    var pages = document.querySelectorAll('.tab-page');
    var found = false;
    for (var i = 0; i < pages.length; i++) {
      var match = pages[i].getAttribute('data-page') === pageId;
      pages[i].classList.toggle('active', match);
      if (match) found = true;
    }
    if (!found && pages.length) {
      pageId = 'install';
      for (var j = 0; j < pages.length; j++) {
        pages[j].classList.toggle('active',
          pages[j].getAttribute('data-page') === pageId);
      }
    }
    var tabs = document.querySelectorAll('#tabBar .tab');
    for (var k = 0; k < tabs.length; k++) {
      tabs[k].classList.toggle('active',
        tabs[k].getAttribute('data-page') === pageId);
    }
    if (persist) localStorage.setItem('tal_patch_tab', pageId);
    window.scrollTo(0, 0);
  }

  // ---------------- 读写 ----------------
  async function loadConfig() {
    try {
      var raw = await ksuBridge.readFile(CONFIG_PATH);
      if (raw) {
        var obj = JSON.parse(raw);
        for (var k in DEFAULTS) {
          if (Object.prototype.hasOwnProperty.call(obj, k)) config[k] = obj[k];
        }
      }
          if (typeof config.notify_app_overrides === 'string') {
          try { config.notify_app_overrides = JSON.parse(config.notify_app_overrides || '{}'); }
          catch (e) { config.notify_app_overrides = {}; }
        }
        initNotifySection();
        $('statusText').textContent = ksuBridge.ksuAvailable()
        ? '已连接 KernelSU' : '未检测到 KernelSU API（预览模式）';
    } catch (e) {
      $('statusText').textContent = '配置读取失败：' + e.message;
    }
    applyTheme(currentTheme(), false);
    render();
  }

  var saveTimer = null;
  function saveConfig(silent) {
    clearTimeout(saveTimer);
    saveTimer = setTimeout(async function () {
      var json = JSON.stringify(config, null, 2);
      try {
        var ok = await ksuBridge.writeFile(CONFIG_PATH, json);
        if (ok) {
          // 同步 /sdcard 镜像供已注入进程热更新，并 bump 版本号
          await ksuBridge.exec(
            'mkdir -p /sdcard/TAL-Patch/state; cp -f "' + CONFIG_PATH + '" "' + MIRROR_PATH +
            '"; chmod 0666 "' + MIRROR_PATH +
            '"; setprop persist.tal_patch.config_version "$(date +%s)"');
          if (!silent) ksuBridge.toast('已保存，热更新将在数秒内生效');
        } else if (!silent) {
          ksuBridge.toast('保存失败');
        }
      } catch (e) {
        if (!silent) ksuBridge.toast('保存失败：' + e.message);
      }
    }, 300);
  }

    // ---------------- 消息通知（应用粒度） ----------------
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

    var PKGINFO_OUT = '/data/adb/modules/tal_patch/cache/apps.json';
    var PKGINFO_CMD = 'rm -f ' + PKGINFO_OUT +
      '; app_process -Djava.class.path=/data/adb/modules/tal_patch/loader.dex' +
      ' /system/bin com.kevin233.talpad.tools.PkgInfo ' + PKGINFO_OUT;

    // 优先用 loader.dex 内置的 PkgInfo 工具导出 图标+应用名，失败回退 pm 纯包名
    async function scanApps() {
      $('appListInfo').textContent = '正在扫描（首次导出图标较慢）…';
      $('btnScanApps').disabled = true;
      var ok = false;
      try {
        var r = await ksuBridge.exec(PKGINFO_CMD, 60000);
        if (r.errno === 0) {
          var raw = await ksuBridge.readFile(PKGINFO_OUT);
          var arr = JSON.parse(raw);
          appList = arr.map(function (o) {
            return { pkg: o.pkg, label: o.label || o.pkg,
                     icon: o.icon || null, system: !!o.system };
          });
          ok = appList.length > 0;
        }
      } catch (e) {
        ok = false;
      }
      if (!ok) {
        try {
          var rUser = await ksuBridge.exec('pm list packages -3');
          var rSys = await ksuBridge.exec('pm list packages -s');
          var seen = {};
          appList = [];
          parsePackageLines(rUser.stdout).forEach(function (p) {
            if (!seen[p]) { seen[p] = 1; appList.push({ pkg: p, label: p, icon: null, system: false }); }
          });
          parsePackageLines(rSys.stdout).forEach(function (p) {
            if (!seen[p]) { seen[p] = 1; appList.push({ pkg: p, label: p, icon: null, system: true }); }
          });
          appList.sort(function (a, b) { return a.pkg < b.pkg ? -1 : 1; });
          ok = appList.length > 0;
          if (ok) ksuBridge.toast('图标导出失败，已回退为纯包名列表');
        } catch (e2) {
          ok = false;
        }
      }
      if (ok) {
        origEffective = {};
        appList.forEach(function (app) { origEffective[app.pkg] = isNotifyOn(app.pkg); });
        recomputeChanged();
        renderAppList();
        updateScopeBar();
        var nu = 0;
        appList.forEach(function (a) { if (!a.system) nu++; });
        $('appListInfo').textContent =
          '共 ' + appList.length + ' 个应用（用户 ' + nu + ' / 系统 ' + (appList.length - nu) + '）';
      } else {
        $('appListInfo').textContent = '扫描失败';
      }
      $('btnScanApps').disabled = false;
    }

    var filterMode = 'all';

    function passFilter(app) {
      switch (filterMode) {
        case 'user': return !app.system;
        case 'system': return app.system;
        case 'on': return isNotifyOn(app.pkg);
        case 'off': return !isNotifyOn(app.pkg);
        case 'changed': return changedApps[app.pkg] !== undefined;
        default: return true;
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
        if (!passFilter(app)) continue;
        if (q && app.pkg.toLowerCase().indexOf(q) < 0 &&
            (app.label || '').toLowerCase().indexOf(q) < 0) continue;
        shown++;
        var row = document.createElement('div');
        row.className = 'row app-row';
        if (app.icon) {
          var img = document.createElement('img');
          img.className = 'app-ico';
          img.src = app.icon;
          img.loading = 'lazy';
          img.alt = '';
          row.appendChild(img);
        }
        var textWrap = document.createElement('div');
        textWrap.className = 'row-text';
        var title = document.createElement('div');
        title.className = 'row-title';
        title.textContent = app.label || app.pkg;
        var badge = document.createElement('span');
        badge.className = 'badge' + (app.system ? ' badge-sys' : '');
        badge.textContent = app.system ? '系统' : '用户';
        title.appendChild(badge);
        textWrap.appendChild(title);
        var sub = document.createElement('div');
        sub.className = 'row-desc app-pkg';
        sub.textContent = app.pkg;
        textWrap.appendChild(sub);
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

  // ---------------- 维护操作 ----------------
  async function doAction(cmd, okMsg) {
    try {
      var r = await ksuBridge.exec(cmd);
      ksuBridge.toast(r.errno === 0 ? okMsg : ('失败：' + (r.stderr || r.stdout || r.errno)));
      return r;
    } catch (e) {
      ksuBridge.toast(e.message);
      return null;
    }
  }

  function bindActions() {
    $('btnRestartSystemUI').onclick = function () {
      doAction('pkill -f com.android.systemui', 'SystemUI 已重启');
    };
    $('btnForceStopTal').onclick = function () {
      doAction('for p in ' + TAL_PACKAGES.join(' ') + '; do am force-stop "$p"; done',
        '已强停全部学习机应用');
    };
    $('btnTestNotify').onclick = function () {
      doAction("cmd notification post -S bigtext -t 'TAL-Patch 测试' tal_patch_test " +
        "'这是 TAL-Patch 的测试通知：下拉查看标题与内容是否完整显示'", '测试通知已发送');
    };
    $('btnViewLog').onclick = async function () {
      var r = await doAction('tail -c 6000 "' + LOG_PATH + '" 2>/dev/null || echo "(暂无日志)"', '已读取日志');
      if (r) {
        $('logBox').textContent = r.stdout || '(暂无日志)';
        $('logDialog').classList.add('show');
      }
    };
    $('btnClearLog').onclick = function () {
      doAction('rm -f "' + LOG_PATH + '" /sdcard/TAL-Patch-Log/* 2>/dev/null', '日志已清空');
    };
    $('btnCloseLog').onclick = function () {
      $('logDialog').classList.remove('show');
    };
    $('notifyAllSwitch').onclick = function () {
        config.notify_all_enabled = !config.notify_all_enabled;
        syncNotifyAllSwitch();
        recomputeChanged();
        saveConfig();
        renderAppList();
        updateScopeBar();
      };
      $('btnScanApps').onclick = scanApps;
      $('appSearchInput').oninput = renderAppList;
      $('appFilterChips').addEventListener('click', function (e) {
        var chip = e.target.closest('.chip');
        if (!chip) return;
        filterMode = chip.getAttribute('data-filter');
        var chips = document.querySelectorAll('#appFilterChips .chip');
        for (var i = 0; i < chips.length; i++) chips[i].classList.remove('active');
        chip.classList.add('active');
        renderAppList();
      });
      $('tabBar').addEventListener('click', function (e) {
        var tab = e.target.closest('.tab');
        if (tab) applyTab(tab.getAttribute('data-page'), true);
      });
      $('btnRestartChanged').onclick = restartChangedApps;
      $('btnDismissScopeBar').onclick = function () {
        changedApps = {};
        updateScopeBar();
      };
      $('themeGroup').addEventListener('click', function (e) {
      var btn = e.target.closest('.seg-btn');
      if (btn) applyTheme(btn.getAttribute('data-value'), true);
    });
  }

  // ---------------- 启动 ----------------
  document.addEventListener('DOMContentLoaded', function () {
    bindActions();
    applyTab(currentTab(), false);
    loadConfig();
    ksuBridge.readFile('/data/adb/modules/tal_patch/module.prop').then(function (raw) {
      var m = raw.match(/^version=(.*)$/m);
      if (m) $('versionText').textContent = m[1].trim();
    }).catch(function () {});
  });
})();

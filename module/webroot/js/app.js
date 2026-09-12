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
    ui_theme: 'miuix'
  };

  var TAL_PACKAGES = [
    'com.tal.pad.studyservice', 'com.tal.dataupload', 'com.tal.pad.backdoor',
    'com.tal.backdoor', 'com.tal.pad.onlineclass', 'com.tal.pad.usercenter',
    'com.tal.pad.minor_protect', 'com.tal.pad.znxxservice'
  ];

  // 配置项 schema：type: switch | text | textarea
  var SCHEMA = [
    { section: '安装与检测' },
    { key: 'unlock_install', type: 'switch', title: '解除学习机系统安装限制',
      desc: '放行任意应用安装：绕过安装白名单、未知来源与 V 型设备限制' },
    { key: 'block_usercenter_detect', type: 'switch', title: '阻止用户中心检测环境并上报日志',
      desc: '屏蔽用户中心的 root / 框架 / 模拟器 / 调试等环境检测与设备上报' },
    { section: '系统行为' },
    { key: 'restore_notification', type: 'switch', title: '恢复通知消息内容',
      desc: '还原被隐藏的通知标题、正文、进度条和操作按钮' },
    { key: 'block_default_wallpaper', type: 'switch', title: '阻止系统恢复默认壁纸',
      desc: '防止系统在定制或开机时把壁纸重置成默认' },
    { key: 'block_default_launcher', type: 'switch', title: '阻止系统恢复默认桌面',
      desc: '防止回桌面时把默认桌面改回 TAL 自带桌面' },
    { section: '学习与应用统计' },
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
    { section: '高级' },
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
    SCHEMA.forEach(function (item) {
      if (item.section) {
        var h = document.createElement('div');
        h.className = 'section-title';
        h.textContent = item.section;
        container.appendChild(h);
        card = document.createElement('div');
        card.className = 'card';
        container.appendChild(card);
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
    $('themeGroup').addEventListener('click', function (e) {
      var btn = e.target.closest('.seg-btn');
      if (btn) applyTheme(btn.getAttribute('data-value'), true);
    });
  }

  // ---------------- 启动 ----------------
  document.addEventListener('DOMContentLoaded', function () {
    bindActions();
    loadConfig();
    ksuBridge.readFile('/data/adb/modules/tal_patch/module.prop').then(function (raw) {
      var m = raw.match(/^version=(.*)$/m);
      if (m) $('versionText').textContent = m[1].trim();
    }).catch(function () {});
  });
})();

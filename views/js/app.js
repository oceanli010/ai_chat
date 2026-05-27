const API_BASE = '';

function showToast(message, type) {
    type = type || 'error';
    const toast = document.createElement('div');
    toast.className = 'toast ' + type;
    toast.textContent = message;
    document.body.appendChild(toast);
    setTimeout(() => toast.remove(), 3000);
}

function handleAuthFailure(message) {
    localStorage.clear();
    try { localStorage.setItem('logout', Date.now()); } catch(e) {}
    showToast(message || '登录已过期，请重新登录', 'error');
    setTimeout(function() { window.location.replace('/login.html'); }, 500);
}

function showBanNotification(data) {
    var existing = document.getElementById('globalBanModal');
    if (existing) existing.remove();

    var modal = document.createElement('div');
    modal.className = 'modal show';
    modal.id = 'globalBanModal';

    var reason = data.ban_reason || '违反平台规则';
    var seconds = data.remaining_seconds || 0;

    var expiryText = '封禁结束时间：';
    if (seconds <= 0) {
        expiryText += '永久封禁';
    } else {
        var now = new Date();
        now.setSeconds(now.getSeconds() + seconds);
        expiryText += formatDate(now.toISOString());
    }

    modal.innerHTML = '' +
        '<div class="modal-content" style="border:2px solid #f44336;max-width:400px;padding:20px;">' +
        '<h3 style="color:#f44336;text-align:center;font-size:18px;margin-bottom:12px;">账号已被封禁</h3>' +
        '<div style="line-height:1.6;">' +
        '<p style="margin-bottom:10px;font-size:14px;">你的账号已被封禁，如有疑问请联系管理员</p>' +
        '<p style="padding:8px 12px;border-radius:6px;font-size:13px;">' + expiryText + '</p>' +
        '<p style="padding:8px 12px;border-radius:6px;margin-top:6px;font-size:13px;"><strong>封禁理由：</strong>' + escapeHtml(reason) + '</p>' +
        '</div>' +
        '<div class="modal-footer" style="justify-content:center;margin-top:14px;">' +
        '<button class="btn-sm" style="min-width:100px;background:var(--primary-gradient);color:#fff;" onclick="dismissBanModal()">确认</button>' +
        '</div>' +
        '</div>';

    document.body.appendChild(modal);
    modal.addEventListener('click', function(e) {
        if (e.target === modal) {
            // prevent close on backdrop click
        }
    });
}

function dismissBanModal() {
    var modal = document.getElementById('globalBanModal');
    if (modal) {
        modal.classList.remove('show');
        setTimeout(function() {
            if (modal.parentNode) modal.parentNode.removeChild(modal);
        }, 400);
    }
    handleAuthFailure('账号已被封禁');
}

function api(method, path, data) {
    const opts = {
        method: method,
        headers: { 'Content-Type': 'application/json' }
    };
    const token = localStorage.getItem('token');
    if (token) opts.headers['Authorization'] = 'Bearer ' + token;
    if (data) opts.body = JSON.stringify(data);
    return fetch(API_BASE + path, opts).then(function(r) {
        return r.json();
    });
}

function formatDate(d) {
    if (!d) return '';
    let s = d.replace('T', ' ').substring(0, 19);
    return s;
}

function escapeHtml(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function parseLogLevel(line) {
    var match = line.match(/\[(\w+)\]/);
    if (match) return match[1].toLowerCase();
    return 'info';
}

function parseLogTime(line) {
    var match = line.match(/\[(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d{3})\]/);
    if (match) return match[1];
    return '';
}

function startPeriodicAuthCheck(intervalMs) {
    intervalMs = intervalMs || 30000;
    setInterval(async function() {
        if (!localStorage.getItem('token') || window._authChecking) return;
        if (document.getElementById('globalBanModal')) return;
        window._authChecking = true;
        try {
            var res = await api('GET', '/api/user/profile');
            if (res.code === 403 && res.data && res.data.banned) {
                showBanNotification(res.data);
            } else if (res.code !== 200) {
                handleAuthFailure('登录已过期，请重新登录');
            }
        } catch (e) {}
        window._authChecking = false;
    }, intervalMs);
}

function setupGlobalLogout() {
    window.addEventListener('storage', function(e) {
        if ((e.key === 'token' && !e.newValue) || e.key === 'logout') {
            if (localStorage.getItem('token')) {
                localStorage.clear();
                window.location.replace('/login.html');
            }
        }
    });
}

function logout() {
    localStorage.clear();
    try { localStorage.setItem('logout', Date.now()); } catch(e) {}
    window.location.replace('/login.html');
}

var wsConnection = null;
var wsReconnectTimer = null;

function connectNotificationWS() {
    var token = localStorage.getItem('token');
    if (!token) return;

    if (wsConnection && (wsConnection.readyState === WebSocket.CONNECTING || wsConnection.readyState === WebSocket.OPEN)) {
        return;
    }

    var wsProtocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    var wsUrl = wsProtocol + '//' + location.host + '/ws/notification?token=' + encodeURIComponent(token);

    try {
        wsConnection = new WebSocket(wsUrl);

        wsConnection.onopen = function() {};

        wsConnection.onmessage = function(event) {
            try {
                var data = JSON.parse(event.data);
                if (data.type === 'ban') {
                    var remainingSeconds = 0;
                    if (data.banned_at && data.duration_hours) {
                        var nowTs = Math.floor(Date.now() / 1000);
                        var bannedAt = parseInt(data.banned_at);
                        var durationSeconds = data.duration_hours * 3600;
                        remainingSeconds = bannedAt + durationSeconds - nowTs;
                        if (remainingSeconds < 0) remainingSeconds = 0;
                    }
                    showBanNotification({
                        ban_reason: data.ban_reason,
                        remaining_seconds: remainingSeconds
                    });
                }
            } catch (e) {}
        };

        wsConnection.onclose = function() {
            wsConnection = null;
            if (localStorage.getItem('token')) {
                wsReconnectTimer = setTimeout(connectNotificationWS, 3000);
            }
        };

        wsConnection.onerror = function() {};
    } catch (e) {}
}

function disconnectNotificationWS() {
    if (wsReconnectTimer) {
        clearTimeout(wsReconnectTimer);
        wsReconnectTimer = null;
    }
    if (wsConnection) {
        wsConnection.onclose = null;
        wsConnection.close();
        wsConnection = null;
    }
}

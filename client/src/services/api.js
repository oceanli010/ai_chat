const BASE_URL = '/api'

async function request(method, path, data = null, customOptions = {}) {
  const config = {
    method,
    headers: {
      'Content-Type': 'application/json'
    }
  }

  if (customOptions.headers) {
    Object.assign(config.headers, customOptions.headers)
  }

  const token = localStorage.getItem('token')
  if (token) {
    config.headers['Authorization'] = `Bearer ${token}`
  }

  if (data && method !== 'GET') {
    config.body = JSON.stringify(data)
  }

  try {
    const url = `${BASE_URL}${path}`
    const response = await fetch(url, config)

    if (!response.ok) {
      const errorData = await response.json().catch(() => ({}))
      const errorMessage = mapErrorMessage(errorData.message) || getHttpErrorMessage(response.status)
      const error = new Error(errorMessage)
      error.code = errorData.code
      error.status = response.status

      if (response.status === 401) {
        localStorage.removeItem('token')
        localStorage.removeItem('user')
        window.location.href = '/login'
        throw error
      }

      throw error
    }

    return response.json()
  } catch (err) {
    if (err.name === 'TypeError' && err.message === 'Failed to fetch') {
      const networkError = new Error('网络连接失败，请检查网络')
      networkError.status = 0
      networkError.code = 'NETWORK_ERROR'
      throw networkError
    }
    throw err
  }
}

function mapErrorMessage(message) {
  const map = {
    'Invalid email or password': '邮箱或密码错误',
    'Account has been banned': '账号已被封禁',
    'Email already registered': '该邮箱已被注册',
    'Invalid verification code': '验证码错误或已过期',
    'Failed to send verification code email': '验证码发送失败，请稍后再试',
    'User not found': '用户不存在',
    'Invalid token': '登录已过期，请重新登录',
    'Service not available': '服务暂时不可用',
    'Redis not available': '服务暂时不可用',
    'User repository not available': '服务暂时不可用',
    'Failed to register user': '注册失败，请稍后再试',
    'Failed to update nickname': '修改昵称失败',
    'Failed to delete account': '删除账号失败',
    'Invalid old password': '旧密码错误',
    'Endpoint not found': '请求的接口不存在',
    'Unauthorized': '没有权限执行此操作',
    'Failed to update user status': '更新用户状态失败',
    'Failed to change password': '修改密码失败，请稍后再试',
    'Failed to reset password': '重置密码失败，请稍后再试',
    'Nickname is required': '昵称不能为空',
    'Password is required': '密码不能为空',
    'Email is required': '邮箱不能为空'
  }
  return map[message] || message
}

function getHttpErrorMessage(status) {
  const messages = {
    400: '请求参数错误',
    401: '登录已过期，请重新登录',
    403: '没有权限执行此操作',
    404: '请求的资源不存在',
    409: '资源冲突',
    429: '请求过于频繁，请稍后再试',
    500: '服务器内部错误',
    502: '服务器暂时不可用',
    503: '服务正在维护'
  }
  return messages[status] || `请求失败 (HTTP ${status})`
}

const api = {
  auth: {
    register(email) {
      return request('POST', '/auth/register', { email })
    },
    verify(email, code, nickname = '', password = '') {
      return request('POST', '/auth/verify', { email, code, nickname, password })
    },
    login(email, password) {
      return request('POST', '/auth/login', { email, password })
    },
    logout() {
      return request('POST', '/auth/logout')
    },
    forgotPassword(email) {
      return request('POST', '/auth/forgot-password', { email })
    },
    resetPassword(email, code, password) {
      return request('POST', '/auth/reset-password', { email, code, password })
    }
  },
  user: {
    getProfile() {
      return request('GET', '/user/profile')
    },
    updateNickname(nickname) {
      return request('PUT', '/user/nickname', { nickname })
    },
    deleteAccount() {
      return request('DELETE', '/user/account')
    },
    changePassword(oldPassword, newPassword) {
      return request('PUT', '/user/password', { old_password: oldPassword, new_password: newPassword })
    }
  },
  chat: {
    getHistory(page = 1, size = 50) {
      return request('GET', `/chat/history?page=${page}&size=${size}`)
    },
    clearHistory() {
      return request('DELETE', '/chat/history')
    }
  },
  admin: {
    getUsers(page = 1, size = 20, filter = '') {
      return request('GET', `/admin/users?page=${page}&size=${size}&filter=${encodeURIComponent(filter)}`)
    },
    banUser(userId, banned) {
      return request('PUT', '/admin/user/ban', { user_id: userId, banned })
    },
    getStats() {
      return request('GET', '/admin/stats')
    }
  }
}

export default api

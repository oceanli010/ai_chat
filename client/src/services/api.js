const BASE_URL = '/api'

async function request(method, path, data = null, options = {}) {
  const config = {
    method,
    headers: {
      'Content-Type': 'application/json',
      ...options.headers
    },
    ...options
  }

  const token = localStorage.getItem('token')
  if (token) {
    config.headers['Authorization'] = `Bearer ${token}`
  }

  if (data && method !== 'GET') {
    config.body = JSON.stringify(data)
  }

  const url = `${BASE_URL}${path}`
  const response = await fetch(url, config)

  if (!response.ok) {
    const errorData = await response.json().catch(() => ({}))
    const error = new Error(errorData.message || `HTTP ${response.status}`)
    error.code = errorData.code
    error.status = response.status
    throw error
  }

  return response.json()
}

const api = {
  auth: {
    register(email, password, nickname) {
      return request('POST', '/auth/register', { email, password, nickname })
    },
    verify(email, code, nickname = '', password = '') {
      return request('POST', '/auth/verify', { email, code, nickname, password })
    },
    login(email, password) {
      return request('POST', '/auth/login', { email, password })
    },
    logout() {
      return request('POST', '/auth/logout')
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
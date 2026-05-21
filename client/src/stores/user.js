import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import api from '../services/api'
import wsClient from '../services/websocket'

export const useUserStore = defineStore('user', () => {
  const user = ref(null)
  const token = ref(localStorage.getItem('token') || '')

  const isLoggedIn = computed(() => !!token.value && !!user.value)
  const isAdmin = computed(() => user.value?.role === 'admin')

  function setAuth(authToken, userData) {
    token.value = authToken
    user.value = userData
    localStorage.setItem('token', authToken)
  }

  function clearAuth() {
    token.value = ''
    user.value = null
    localStorage.removeItem('token')
  }

  async function login(email, password) {
    const result = await api.auth.login(email, password)
    setAuth(result.token, result.user)
    return result
  }

  async function register(email) {
    return await api.auth.register(email)
  }

  async function verifyEmail(email, code, nickname, password) {
    const result = await api.auth.verify(email, code, nickname, password)
    setAuth(result.token, result.user)
    return result
  }

  async function logout() {
    wsClient.disconnect()
    try {
      await api.auth.logout()
    } catch (e) {
      // ignore
    }
    clearAuth()
  }

  async function fetchProfile() {
    const result = await api.user.getProfile()
    user.value = result.user
    return result
  }

  return {
    user,
    token,
    isLoggedIn,
    isAdmin,
    login,
    register,
    verifyEmail,
    logout,
    fetchProfile,
    setAuth,
    clearAuth
  }
})
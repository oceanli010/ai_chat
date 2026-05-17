<template>
  <div class="auth-page">
    <div class="auth-card">
      <h1 class="auth-title">注册新账号</h1>

      <div v-if="step === 1">
        <form @submit.prevent="sendCode" class="auth-form">
          <div class="form-group">
            <label for="email">邮箱</label>
            <input
              id="email"
              v-model="form.email"
              type="email"
              placeholder="请输入邮箱"
              required
            />
          </div>
          <div v-if="error" class="error-message">{{ error }}</div>
          <div v-if="successMsg" class="success-message">{{ successMsg }}</div>
          <button type="submit" class="btn btn-primary" :disabled="loading || countdown > 0">
            {{ countdown > 0 ? `${countdown}s 后可重发` : '发送验证码' }}
          </button>
        </form>
      </div>

      <div v-if="step === 2">
        <form @submit.prevent="handleRegister" class="auth-form">
          <div class="form-group">
            <label for="code">验证码</label>
            <input
              id="code"
              v-model="form.code"
              type="text"
              placeholder="请输入6位验证码"
              maxlength="6"
              required
            />
          </div>
          <div class="form-group">
            <label for="nickname">昵称</label>
            <input
              id="nickname"
              v-model="form.nickname"
              type="text"
              placeholder="请输入昵称"
              maxlength="32"
              required
            />
          </div>
          <div class="form-group">
            <label for="password">密码</label>
            <input
              id="password"
              v-model="form.password"
              type="password"
              placeholder="请输入密码（至少6位）"
              minlength="6"
              required
            />
          </div>
          <div v-if="error" class="error-message">{{ error }}</div>
          <button type="submit" class="btn btn-primary" :disabled="loading">
            {{ loading ? '注册中...' : '完成注册' }}
          </button>
          <button type="button" class="btn btn-secondary" @click="step = 1">返回修改邮箱</button>
        </form>
      </div>

      <p class="auth-switch">
        已有账号？<router-link to="/login">立即登录</router-link>
      </p>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'

const router = useRouter()
const userStore = useUserStore()

const step = ref(1)
const loading = ref(false)
const error = ref('')
const successMsg = ref('')
const countdown = ref(0)

const form = reactive({
  email: '',
  code: '',
  password: '',
  nickname: ''
})

let countdownTimer = null

async function sendCode() {
  error.value = ''
  successMsg.value = ''
  loading.value = true
  try {
    await userStore.register(form.email, '', '')
    successMsg.value = '验证码已发送到您的邮箱'
    step.value = 2
    startCountdown()
  } catch (e) {
    error.value = e.message || '发送验证码失败'
  } finally {
    loading.value = false
  }
}

function startCountdown() {
  countdown.value = 60
  countdownTimer = setInterval(() => {
    countdown.value--
    if (countdown.value <= 0) {
      clearInterval(countdownTimer)
      countdownTimer = null
    }
  }, 1000)
}

async function handleRegister() {
  error.value = ''
  loading.value = true
  try {
    await userStore.verifyEmail(form.email, form.code, form.nickname, form.password)
    router.push('/chat')
  } catch (e) {
    error.value = e.message || '注册失败，请检查验证码'
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.auth-page {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
}

.auth-card {
  width: 100%;
  max-width: 400px;
  background: #fff;
  border-radius: 12px;
  padding: 40px 32px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.08);
}

.auth-title {
  font-size: 24px;
  font-weight: 600;
  text-align: center;
  margin-bottom: 32px;
  color: #1a1a1a;
}

.auth-form {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.form-group label {
  font-size: 14px;
  font-weight: 500;
  color: #555;
}

.form-group input {
  height: 44px;
  padding: 0 12px;
  border: 1px solid #d9d9d9;
  border-radius: 8px;
  font-size: 14px;
  transition: border-color 0.2s;
  outline: none;
}

.form-group input:focus {
  border-color: #4f46e5;
  box-shadow: 0 0 0 2px rgba(79, 70, 229, 0.15);
}

.error-message {
  padding: 10px 12px;
  background: #fef2f2;
  color: #dc2626;
  border-radius: 8px;
  font-size: 13px;
}

.success-message {
  padding: 10px 12px;
  background: #f0fdf4;
  color: #16a34a;
  border-radius: 8px;
  font-size: 13px;
}

.btn {
  height: 44px;
  border: none;
  border-radius: 8px;
  font-size: 15px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
}

.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.btn-primary {
  background: #4f46e5;
  color: #fff;
}

.btn-primary:hover:not(:disabled) {
  background: #4338ca;
}

.btn-secondary {
  background: #f3f4f6;
  color: #555;
}

.btn-secondary:hover {
  background: #e5e7eb;
}

.auth-switch {
  margin-top: 24px;
  text-align: center;
  font-size: 14px;
  color: #888;
}

.auth-switch a {
  color: #4f46e5;
  text-decoration: none;
  font-weight: 500;
}

.auth-switch a:hover {
  text-decoration: underline;
}
</style>
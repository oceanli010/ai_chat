<template>
  <div class="chat-page">
    <div class="chat-header">
      <h2>AI 聊天室</h2>
      <div class="chat-header-actions">
        <button class="btn-icon" @click="clearHistory" title="清除聊天记录">🗑</button>
      </div>
    </div>

    <div class="ws-status-bar">
      <div v-if="wsError" class="ws-error-bar" @click="dismissError">
        <span class="ws-error-text">{{ wsError }}</span>
        <span class="ws-error-close">&times;</span>
      </div>
      <div v-if="reconnectCount > 0" class="ws-reconnect-bar">
        Reconnecting... ({{ reconnectCount }})
      </div>
    </div>

    <div class="messages-container" ref="messagesContainer">
      <div v-if="loadingHistory" class="empty-chat">
        <div class="loading-spinner"></div>
        <p>加载聊天记录中...</p>
      </div>
      <div v-else-if="messages.length === 0" class="empty-chat">
        <div class="empty-icon">💬</div>
        <p>开始和 AI 对话吧</p>
      </div>
      <MessageBubble
        v-for="msg in messages"
        :key="msg.id"
        :message="msg"
      />
      <div v-if="streaming" class="streaming-indicator">
        <span class="typing-dot"></span>
        <span class="typing-dot"></span>
        <span class="typing-dot"></span>
      </div>
    </div>

    <ChatInput
      :disabled="!wsConnected || streaming"
      :placeholder="wsConnected ? '输入消息...' : '连接中...'"
      @send="handleSendMessage"
    />
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted, nextTick, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'
import api from '../services/api'
import wsClient from '../services/websocket'
import MessageBubble from '../components/MessageBubble.vue'
import ChatInput from '../components/ChatInput.vue'

const router = useRouter()
const userStore = useUserStore()

const messagesContainer = ref(null)
const messages = ref([])
const streaming = ref(false)
const wsConnected = ref(false)
const wsError = ref('')
const reconnectCount = ref(0)
const loadingHistory = ref(false)
const nextId = ref(1)
let wsErrorTimer = null

function generateId() {
  return `msg_${Date.now()}_${nextId.value++}`
}

function scrollToBottom() {
  nextTick(() => {
    if (messagesContainer.value) {
      messagesContainer.value.scrollTop = messagesContainer.value.scrollHeight
    }
  })
}

function addMessage(role, content) {
  messages.value.push({
    id: generateId(),
    role,
    content,
    timestamp: Date.now()
  })
  scrollToBottom()
}

function handleSendMessage(content) {
  if (!content.trim()) return

  addMessage('user', content.trim())

  wsClient.send('chat_message', {
    content: content.trim(),
    timestamp: Math.floor(Date.now() / 1000)
  })

  streaming.value = true
  scrollToBottom()
}

function handleChatResponse(data) {
  streaming.value = false
  addMessage('assistant', data.content)
}

function handleError(data) {
  streaming.value = false
  wsError.value = data?.message || 'WebSocket connection error'
  if (wsErrorTimer) clearTimeout(wsErrorTimer)
  wsErrorTimer = setTimeout(() => {
    wsError.value = ''
    wsErrorTimer = null
  }, 5000)
}

function dismissError() {
  wsError.value = ''
  if (wsErrorTimer) {
    clearTimeout(wsErrorTimer)
    wsErrorTimer = null
  }
}

async function clearHistory() {
  try {
    await api.chat.clearHistory()
    messages.value = []
  } catch (e) {
    console.error('Failed to clear history:', e)
  }
}

async function loadHistory() {
  loadingHistory.value = true
  try {
    const result = await api.chat.getHistory(1, 100)
    if (result.messages && Array.isArray(result.messages)) {
      messages.value = result.messages.map((msg) => ({
        id: msg.id || generateId(),
        role: msg.role,
        content: msg.content,
        timestamp: msg.created_at ? new Date(msg.created_at).getTime() : Date.now()
      }))
    }
  } catch (e) {
    console.error('Failed to load history:', e)
  } finally {
    loadingHistory.value = false
  }
  scrollToBottom()
}

onMounted(async () => {
  if (!userStore.token) {
    router.push('/login')
    return
  }

  try {
    await userStore.fetchProfile()
  } catch (e) {
    console.error('Failed to fetch profile:', e)
  }

  await loadHistory()

  wsClient.on('open', () => {
    wsConnected.value = true
    reconnectCount.value = 0
    loadHistory()
  })
  wsClient.on('close', () => { wsConnected.value = false })
  wsClient.on('max_reconnect', () => {
    wsConnected.value = false
    wsError.value = 'Unable to connect to server, please refresh the page'
    if (wsErrorTimer) clearTimeout(wsErrorTimer)
    wsErrorTimer = null
  })
  wsClient.on('reconnect', (data) => { reconnectCount.value = data.attempt })

  wsClient.on('chat_response', handleChatResponse)
  wsClient.on('error', handleError)

  wsClient.connect(userStore.token)
  scrollToBottom()
})

onUnmounted(() => {
  wsClient.off('open')
  wsClient.off('close')
  wsClient.off('max_reconnect')
  wsClient.off('reconnect')
  wsClient.off('chat_response')
  wsClient.off('error')
  wsClient.disconnect()
  if (wsErrorTimer) {
    clearTimeout(wsErrorTimer)
    wsErrorTimer = null
  }
})
</script>

<style scoped>
.chat-page {
  flex: 1;
  display: flex;
  flex-direction: column;
  max-height: 100vh;
}

.chat-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 20px;
  background: #fff;
  border-bottom: 1px solid #e5e7eb;
  flex-shrink: 0;
}

.chat-header h2 {
  font-size: 18px;
  font-weight: 600;
  color: #1a1a1a;
}

.chat-header-actions {
  display: flex;
  gap: 8px;
}

.btn-icon {
  width: 36px;
  height: 36px;
  border: none;
  border-radius: 8px;
  background: #f3f4f6;
  font-size: 16px;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background 0.2s;
}

.btn-icon:hover {
  background: #e5e7eb;
}

.messages-container {
  flex: 1;
  overflow-y: auto;
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.empty-chat {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #aaa;
}

.empty-icon {
  font-size: 64px;
  margin-bottom: 16px;
}

.empty-chat p {
  font-size: 16px;
}

.loading-spinner {
  width: 32px;
  height: 32px;
  border: 3px solid #e5e7eb;
  border-top-color: #4f46e5;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
  margin-bottom: 12px;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.streaming-indicator {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 8px 16px;
  align-self: flex-start;
}

.typing-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #ccc;
  animation: typing 1.4s infinite ease-in-out both;
}

.typing-dot:nth-child(1) { animation-delay: 0s; }
.typing-dot:nth-child(2) { animation-delay: 0.2s; }
.typing-dot:nth-child(3) { animation-delay: 0.4s; }

@keyframes typing {
  0%, 80%, 100% {
    transform: scale(0.6);
    opacity: 0.4;
  }
  40% {
    transform: scale(1);
    opacity: 1;
  }
}

.ws-status-bar {
  flex-shrink: 0;
}

.ws-error-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 20px;
  background: rgba(239, 68, 68, 0.12);
  border-bottom: 1px solid rgba(239, 68, 68, 0.25);
  cursor: pointer;
  transition: background 0.2s;
}

.ws-error-bar:hover {
  background: rgba(239, 68, 68, 0.18);
}

.ws-error-text {
  font-size: 14px;
  color: #b91c1c;
  line-height: 1.4;
}

.ws-error-close {
  font-size: 20px;
  color: #b91c1c;
  margin-left: 12px;
  flex-shrink: 0;
  opacity: 0.6;
}

.ws-error-close:hover {
  opacity: 1;
}

.ws-reconnect-bar {
  padding: 6px 20px;
  text-align: center;
  font-size: 13px;
  color: #d97706;
  background: rgba(245, 158, 11, 0.1);
  border-bottom: 1px solid rgba(245, 158, 11, 0.2);
}
</style>
<template>
  <div class="chat-page">
    <div class="chat-header">
      <h2>AI 聊天室</h2>
      <div class="chat-header-actions">
        <button class="btn-icon" @click="clearHistory" title="清除聊天记录">🗑</button>
      </div>
    </div>

    <div class="messages-container" ref="messagesContainer">
      <div v-if="messages.length === 0" class="empty-chat">
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
const nextId = ref(1)

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
  addMessage('assistant', `[错误] ${data.message || '未知错误'}`)
}

async function clearHistory() {
  try {
    await api.chat.clearHistory()
    messages.value = []
  } catch (e) {
    console.error('Failed to clear history:', e)
  }
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
  }

  wsClient._on('open', () => { wsConnected.value = true })
  wsClient._on('close', () => { wsConnected.value = false })
  wsClient._on('max_reconnect', () => { wsConnected.value = false })

  wsClient.on('chat_response', handleChatResponse)
  wsClient.on('error', handleError)

  wsClient.connect(userStore.token)
  scrollToBottom()
})

onUnmounted(() => {
  wsClient.off('chat_response')
  wsClient.off('error')
  wsClient.disconnect()
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
</style>
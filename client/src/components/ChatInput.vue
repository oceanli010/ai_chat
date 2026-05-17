<template>
  <div class="chat-input-wrapper">
    <div class="chat-input-container">
      <textarea
        ref="inputRef"
        v-model="text"
        :placeholder="placeholder"
        :disabled="disabled"
        class="chat-input"
        rows="1"
        @keydown.enter.exact.prevent="handleSend"
        @input="autoResize"
      ></textarea>
      <button
        class="send-btn"
        :disabled="disabled || !text.trim()"
        @click="handleSend"
      >发送</button>
    </div>
  </div>
</template>

<script setup>
import { ref, nextTick } from 'vue'

const props = defineProps({
  disabled: {
    type: Boolean,
    default: false
  },
  placeholder: {
    type: String,
    default: '输入消息...'
  }
})

const emit = defineEmits(['send'])

const text = ref('')
const inputRef = ref(null)

function handleSend() {
  const content = text.value.trim()
  if (!content || props.disabled) return
  emit('send', content)
  text.value = ''
  nextTick(() => {
    autoResize()
  })
}

function autoResize() {
  if (inputRef.value) {
    inputRef.value.style.height = 'auto'
    inputRef.value.style.height = Math.min(inputRef.value.scrollHeight, 120) + 'px'
  }
}
</script>

<style scoped>
.chat-input-wrapper {
  flex-shrink: 0;
  padding: 12px 20px 20px;
  background: #fff;
  border-top: 1px solid #e5e7eb;
}

.chat-input-container {
  display: flex;
  gap: 10px;
  align-items: flex-end;
  padding: 10px 14px;
  background: #f8f8fa;
  border-radius: 12px;
  border: 1px solid #e5e7eb;
  transition: border-color 0.2s;
}

.chat-input-container:focus-within {
  border-color: #4f46e5;
  box-shadow: 0 0 0 2px rgba(79, 70, 229, 0.1);
}

.chat-input {
  flex: 1;
  border: none;
  outline: none;
  background: transparent;
  font-size: 14px;
  line-height: 1.5;
  resize: none;
  max-height: 120px;
  min-height: 24px;
  font-family: inherit;
}

.chat-input:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.chat-input::placeholder {
  color: #bbb;
}

.send-btn {
  height: 34px;
  padding: 0 18px;
  border: none;
  border-radius: 8px;
  background: #4f46e5;
  color: #fff;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
  flex-shrink: 0;
}

.send-btn:hover:not(:disabled) {
  background: #4338ca;
}

.send-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>
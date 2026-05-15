import type { ConnectionUpdatePacket } from '@/types/connection.ts'


async function fetchConnectionStatus(): Promise<ConnectionUpdatePacket> {
  const response = await fetch('/api/status')
  if (!response.ok) {
    throw new Error(`HTTP error! status: ${response.status}`)
  }
  return await response.json() as ConnectionUpdatePacket
}

export { fetchConnectionStatus }

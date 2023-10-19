import type { Metadata } from 'next'
import { Inter } from 'next/font/google'
import './globals.css'
import Provider from './providers'


const inter = Inter({ subsets: ['latin'] })

export const metadata: Metadata = {
  title: 'lambadex',
  description: 'lambadex cartesi dapp',
}

export default function RootLayout() {
  return (
    <html lang="en"><body className={inter.className}>
      <Provider />
    </body></html>
  )
}

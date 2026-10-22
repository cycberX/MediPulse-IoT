import express from 'express';
import cors from 'cors';
import Database from 'better-sqlite3';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const PORT = process.env.PORT || 3000;

// Initialize SQLite database
const db = new Database('doses.db');

// Create table if not exists
db.exec(`
  CREATE TABLE IF NOT EXISTS dose_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL,
    window TEXT NOT NULL,
    date_taken TEXT NOT NULL,
    status TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(code, window, date_taken)
  );
`);

// Prepared SQLite statements
const insertDose = db.prepare(`
  INSERT INTO dose_logs (code, window, date_taken, status)
  VALUES (@code, @window, @date_taken, @status)
  ON CONFLICT(code, window, date_taken) DO UPDATE SET status = excluded.status;
`);

const checkDose = db.prepare(`
  SELECT status FROM dose_logs 
  WHERE code = ? AND window = ? AND date_taken = ?
`);

const getLogsByCode = db.prepare(`
  SELECT code, window, date_taken, status, created_at 
  FROM dose_logs 
  WHERE code = ?
  ORDER BY id DESC LIMIT 50
`);

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

function getISTDetails() {
  const now = new Date();
  const istDateStr = now.toLocaleDateString('en-CA', { timeZone: 'Asia/Kolkata' }); // YYYY-MM-DD
  const istTimeStr = now.toLocaleTimeString('en-GB', { timeZone: 'Asia/Kolkata', hour12: false }); // HH:MM:SS
  const hour = parseInt(istTimeStr.split(':')[0], 10);

  return { todayDate: istDateStr, hour, ist_time: istTimeStr };
}

function computeProfileState(code) {
  const { todayDate, hour, ist_time } = getISTDetails();

  let active_window = 'morning';
  let status = 'PENDING';
  let buzzer = false;

  // Rule logic applied for profiles (including 1552)
  if (hour >= 5 && hour < 10) {
    // 5:00 AM - 10:59 AM: Pending
    active_window = 'morning';
    status = 'PENDING';
    buzzer = false;
  } else if (hour >= 10 && hour < 17) {
    // 11:00 AM - 4:59 PM: Morning window
    active_window = 'morning';
    const record = checkDose.get(code, 'morning', todayDate);
    const isTaken = record && record.status === 'TAKEN';
    status = isTaken ? 'TAKEN' : 'NOT_TAKEN';
    buzzer = !isTaken;
  } else if (hour >= 17 && hour < 20) {
    // 5:00 PM - 7:59 PM: Reset at 5 PM, waiting for night buzzer
    active_window = 'night';
    status = 'PENDING';
    buzzer = false;
  } else if (hour >= 20 && hour < 22) {
    // 8:00 PM - 9:59 PM: Night window
    active_window = 'night';
    const record = checkDose.get(code, 'night', todayDate);
    const isTaken = record && record.status === 'TAKEN';
    status = isTaken ? 'TAKEN' : 'NOT_TAKEN';
    buzzer = !isTaken;
  } else {
    // 10:00 PM - 4:59 AM: Cutoff missed
    active_window = 'night';
    const record = checkDose.get(code, 'night', todayDate);
    const isTaken = record && record.status === 'TAKEN';
    status = isTaken ? 'TAKEN' : 'MISSED';
    buzzer = false;
  }

  return { code, buzzer, status, active_window, ist_time };
}

// 1. GET Status
app.get('/api/get_status', (req, res) => {
  const { code } = req.query;
  if (!code) return res.status(400).json({ error: 'Missing code' });
  return res.status(200).json(computeProfileState(code));
});

// 2. POST Status
app.post('/api/post_status', (req, res) => {
  const code = req.query.code || req.body.code;
  const status = (req.query.status || req.body.status || '').toUpperCase();
  const time = (req.query.time || req.body.time || '').toLowerCase();

  if (!code || !status || !time) {
    return res.status(400).json({ error: 'Missing required parameters' });
  }

  const { todayDate } = getISTDetails();

  insertDose.run({
    code,
    window: time,
    date_taken: todayDate,
    status,
  });

  return res.status(200).json({ success: true, code, status, time, date: todayDate });
});

// 3. GET Filtered Logs for specific code
app.get('/api/logs', (req, res) => {
  const { code } = req.query;
  if (!code) return res.status(400).json({ error: 'Missing code' });
  const logs = getLogsByCode.all(code);
  return res.status(200).json(logs);
});

app.get('/health', (req, res) => {
  res.status(200).json({ status: 'ok', uptime: process.uptime() });
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Server listening on port ${PORT}`);

  // URL of your deployed Render app (Set this in Render Environment Variables)
  // e.g. RENDER_EXTERNAL_URL = https://smart-buzzer.onrender.com
  const APP_URL = process.env.RENDER_EXTERNAL_URL;

  if (APP_URL) {
    const PING_INTERVAL_MS = 13 * 60 * 1000; // 13 minutes (Render sleeps at 15 minutes)

    setInterval(async () => {
      try {
        const response = await fetch(`${APP_URL}/health`);
        console.log(`[Keep-Alive] Ping sent to ${APP_URL}/health - Status: ${response.status}`);
      } catch (err) {
        console.error('[Keep-Alive] Ping failed:', err.message);
      }
    }, PING_INTERVAL_MS);

    console.log(`[Keep-Alive] Self-ping active for ${APP_URL} every 13 minutes.`);
  } else {
    console.log('[Keep-Alive] No RENDER_EXTERNAL_URL set. Self-ping skipped in local mode.');
  }
});
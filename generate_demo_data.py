import os
import sys
import sqlite3
import random
import time
import uuid
from datetime import datetime

def get_db_path():
    db_dir = os.path.join(os.path.expanduser("~"), ".fsm_index")
    os.makedirs(db_dir, exist_ok=True)
    return os.path.join(db_dir, "files_web.db")

def setup_database(db_path):
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Check if table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='files'")
    if cursor.fetchone():
        cursor.execute("SELECT COUNT(*) FROM files")
        count = cursor.fetchone()[0]
        if count > 0:
            print(f"Database at {db_path} already contains {count} records.")
            ans = input("Do you want to overwrite existing data? (y/N): ")
            if ans.lower() != 'y':
                print("Aborting.")
                sys.exit(0)
            print("Clearing existing records...")
            cursor.execute("DELETE FROM files")
            conn.commit()
    else:
        cursor.execute("""
            CREATE TABLE files (
                filepath TEXT PRIMARY KEY,
                filename TEXT,
                size INT,
                mtime INT,
                parent_dir TEXT,
                trueType TEXT,
                entropy REAL,
                checksum TEXT,
                indexed_at INT
            )
        """)
        conn.commit()
    
    return conn

# Realistic folder structure for the DB
DB_ROOT = "C:\\Users\\Demo"
FOLDERS = [
    "Documents\\Work\\Reports", "Documents\\Work\\Invoices", "Documents\\Personal",
    "Downloads\\Software", "Downloads\\Torrents", "Downloads\\Images", "Downloads\\Documents",
    "Pictures\\2023\\Vacation", "Pictures\\2023\\Family", "Pictures\\2024\\Party", "Pictures\\Memes",
    "Pictures\\Wallpapers", "Pictures\\Screenshots",
    "Videos\\Movies", "Videos\\TV_Shows", "Videos\\Recordings", "Videos\\Youtube",
    "Music\\Rock", "Music\\Pop", "Music\\Classical", "Music\\Jazz", "Music\\Podcasts",
    "AppData\\Cache", "AppData\\Local\\Temp", "AppData\\Roaming\\Discord", "AppData\\Local\\Google",
    "Development\\Python", "Development\\JavaScript", "Development\\Cpp\\Engine", "Development\\Rust",
    "Desktop\\Shortcuts", "Desktop\\Temp", "Desktop\\Work",
    "Games\\Steam\\steamapps", "Games\\Epic", "Games\\Origin",
    "Work\\2023\\Q1", "Work\\2023\\Q2", "Work\\2024\\Q1", "Work\\2024\\Q2",
    "Backups\\Phone", "Backups\\PC", "Backups\\Website",
    "Projects\\DataStruct_FSM", "Projects\\ReactApp", "Projects\\API",
    "Library\\Books", "Library\\Manuals", "Library\\References"
]

FILE_TYPES = [
    # ext, trueType, min_size, max_size
    ("jpg", "JPEG", 1024**2, 8*1024**2),
    ("png", "PNG", 1024**2, 8*1024**2),
    ("pdf", "PDF", 100*1024, 50*1024**2),
    ("zip", "ZIP/OFFICE", 1024**2, 2*1024**3),
    ("exe", "EXE/DLL", 1024**2, 500*1024**2),
    ("mp4", "DATA", 200*1024**2, 2*1024**3),
    ("mkv", "DATA", 200*1024**2, 2*1024**3),
    ("py", "DATA", 1024, 500*1024),
    ("js", "DATA", 1024, 500*1024),
    ("cpp", "DATA", 1024, 500*1024),
    ("docx", "ZIP/OFFICE", 50*1024, 10*1024**2),
    ("xlsx", "ZIP/OFFICE", 50*1024, 10*1024**2),
    ("log", "DATA", 10*1024, 100*1024**2)
]

def generate_records():
    TOTAL_FILES = 180000
    NUM_DUPES = int(TOTAL_FILES * 0.08)
    NUM_HIGH_ENTROPY = int(TOTAL_FILES * 0.03)
    NUM_OLD = int(TOTAL_FILES * 0.25)
    
    NUM_BASE = TOTAL_FILES - NUM_DUPES
    
    # Old files: older than 2 years ago (2020 to 2022)
    start_old = int(datetime(2020, 1, 1).timestamp())
    end_old = int(datetime(2022, 12, 31).timestamp())
    
    # Normal files: spread mtime values across all 12 months of 2023 and 2024
    start_new = int(datetime(2023, 1, 1).timestamp())
    end_new = int(datetime(2024, 12, 31).timestamp())
    
    now = int(time.time())
    
    records = []
    base_pool_for_dupes = []
    
    print("Generating base files in memory...")
    filepaths_set = set()
    
    def generate_random_filename(ext):
        words = ["report", "data", "backup", "image", "video", "doc", "project", "final", "test", "log", "cache", "temp", "setup", "main", "utils"]
        return f"{random.choice(words)}_{random.randint(1000, 99999)}_{uuid.uuid4().hex[:6]}.{ext}"

    for i in range(NUM_BASE):
        folder = random.choice(FOLDERS)
        parent_dir = os.path.join(DB_ROOT, folder)
        ext, trueType, min_sz, max_sz = random.choice(FILE_TYPES)
        
        filename = generate_random_filename(ext)
        filepath = os.path.join(parent_dir, filename)
        
        while filepath in filepaths_set:
            filename = generate_random_filename(ext)
            filepath = os.path.join(parent_dir, filename)
            
        filepaths_set.add(filepath)
        size = random.randint(min_sz, max_sz)
        
        if i < NUM_HIGH_ENTROPY:
            entropy = random.uniform(7.51, 8.0)
        else:
            entropy = random.uniform(1.0, 7.49)
            
        if i < NUM_OLD:
            mtime = random.randint(start_old, end_old)
        else:
            mtime = random.randint(start_new, end_new)
            
        checksum = uuid.uuid4().hex
        indexed_at = now
        
        record = (filepath, filename, size, mtime, parent_dir, trueType, entropy, checksum, indexed_at)
        records.append(record)
        
        if len(base_pool_for_dupes) < NUM_DUPES:
            base_pool_for_dupes.append(record)
            
    print("Generating duplicates...")
    for base_record in base_pool_for_dupes:
        _, filename, size, mtime, _, trueType, entropy, checksum, _ = base_record
        
        folder = random.choice(FOLDERS)
        parent_dir = os.path.join(DB_ROOT, folder)
        filepath = os.path.join(parent_dir, filename)
        
        while filepath in filepaths_set:
            folder = random.choice(FOLDERS)
            parent_dir = os.path.join(DB_ROOT, folder)
            filepath = os.path.join(parent_dir, filename)
            
        filepaths_set.add(filepath)
        
        new_record = (filepath, filename, size, mtime, parent_dir, trueType, entropy, checksum, now)
        records.append(new_record)
        
    random.shuffle(records)
    return records

def insert_records(conn, records):
    print("Inserting into database...")
    cursor = conn.cursor()
    batch_size = 50000
    total = len(records)
    
    for i in range(0, total, batch_size):
        batch = records[i:i+batch_size]
        cursor.executemany("""
            INSERT INTO files (filepath, filename, size, mtime, parent_dir, trueType, entropy, checksum, indexed_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, batch)
        conn.commit()
        print(f"  Inserted {min(i+batch_size, total)} / {total} records")

def create_real_files():
    base_path = "C:\\DemoStorage" if os.name == "nt" else os.path.join(os.path.expanduser("~"), "DemoStorage")
    print(f"Creating real files at {base_path}...")
    
    TOTAL_REAL_FILES = 5000
    created = 0
    
    for folder in FOLDERS:
        os.makedirs(os.path.join(base_path, folder), exist_ok=True)
        
    for i in range(TOTAL_REAL_FILES):
        folder = random.choice(FOLDERS)
        ext, _, _, _ = random.choice(FILE_TYPES)
        filename = f"demo_file_{i}_{uuid.uuid4().hex[:4]}.{ext}"
        filepath = os.path.join(base_path, folder, filename)
        
        try:
            with open(filepath, "wb") as f:
                f.write(b"demo data")
            created += 1
        except Exception:
            pass
            
        if (i+1) % 1000 == 0:
            print(f"  Created {i+1} / {TOTAL_REAL_FILES} real files")
            
    return created, base_path

def main():
    start_time = time.time()
    
    db_path = get_db_path()
    conn = setup_database(db_path)
    
    records = generate_records()
    insert_records(conn, records)
    
    conn.close()
    
    real_files_count, scan_path = create_real_files()
    
    elapsed = time.time() - start_time
    
    print("\n" + "="*40)
    print(" SUMMARY")
    print("="*40)
    print(f" ✓ {len(records)} files inserted into DB")
    print(f" ✓ {real_files_count} real files created at {scan_path}")
    print(f" ✓ Total runtime: {elapsed:.2f} seconds")
    print("\n → Now run: python api.py")
    print(f" → Then in the UI scan: {scan_path}")
    print("="*40)

if __name__ == "__main__":
    main()

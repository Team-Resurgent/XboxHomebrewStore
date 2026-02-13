import json
import random
import string
from datetime import datetime, timedelta

# ----------------------------
# Configuration Pools
# ----------------------------

CATEGORIES = ["games", "emulators", "tools", "media", "misc", "saves"]

REGIONS = ["USA", "PAL", "JPN", "USA-PAL", "USA-JPN", "GLO"]

AUTHORS = [
    "XBDev", "Homebrew Team", "OpenSoft",
    "Retro Labs", "Indie Dev", "XBTools",
    "GameStudio", "MediaGroup", "NetSoft",
    "NextGen Coders", "ArcadeForge"
]

CHANGELOGS = [
    "Initial release",
    "Bug fixes and stability improvements",
    "Performance optimizations",
    "Updated core engine",
    "Improved compatibility with more titles",
    "Major rewrite with enhancements",
    "Security improvements",
    "UI overhaul and better navigation"
]

NAME_PREFIXES = [
    "Ultra", "Hyper", "Neo", "Super", "Advanced",
    "Pro", "Next", "Retro", "Dynamic", "Smart"
]

NAME_SUFFIXES = [
    "Launcher", "Studio", "Manager", "Player",
    "Dashboard", "Explorer", "Engine", "Suite",
    "Toolkit", "Center"
]

DESCRIPTION_TEMPLATES = [
    "A powerful {category} application for Xbox.",
    "Next-generation {category} experience.",
    "Optimized {category} software with enhanced features.",
    "High performance {category} app built for stability.",
    "All-in-one {category} solution."
]

# ----------------------------
# Global Uniqueness Tracking
# ----------------------------

used_app_ids = set()
used_version_guids = set()


# ----------------------------
# Helper Functions
# ----------------------------

def random_string(length=6):
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))


def random_hex(length=8):
    return ''.join(random.choices("0123456789ABCDEF", k=length))


def generate_unique_app_id(name):
    while True:
        new_id = f"{name.lower().replace(' ', '-')}-{random_string(4)}"
        if new_id not in used_app_ids:
            used_app_ids.add(new_id)
            return new_id


def generate_unique_guid():
    while True:
        guid = str(random.randint(1000000, 999999999))
        if guid not in used_version_guids:
            used_version_guids.add(guid)
            return guid


def random_date():
    start_date = datetime.now() - timedelta(days=365)
    random_days = random.randint(0, 365)
    date = start_date + timedelta(days=random_days)
    return date.strftime("%b %d, %Y")


def random_size():
    return random.randint(256 * 1024, 50 * 1024 * 1024)


def generate_version(version_number):
    return {
        "guid": generate_unique_guid(),  # 🔥 unique per version
        "version": version_number,
        "size": random_size(),
        "state": 0,
        "release_date": random_date(),
        "changelog": random.choice(CHANGELOGS),
        "title_id": random_hex(8),
        "region": random.choice(REGIONS)
    }


def generate_versions():
    version_count = random.randint(1, 5)
    versions = []

    major = random.randint(1, 3)
    minor = random.randint(0, 5)

    for i in range(version_count):
        version_str = f"{major}.{minor + i}"
        versions.append(generate_version(version_str))

    return versions


def generate_name():
    return f"{random.choice(NAME_PREFIXES)} {random.choice(NAME_SUFFIXES)}"


def generate_app(index):
    category = random.choice(CATEGORIES)
    name = generate_name()

    return {
        "id": generate_unique_app_id(name),  # 🔒 guaranteed unique
        "name": name,
        "author": random.choice(AUTHORS),
        "category": category,
        "description": random.choice(DESCRIPTION_TEMPLATES).format(category=category),
        "new": random.choice([True, False]),
        "versions": generate_versions()  # GUID now inside versions
    }


def generate_json(app_count):
    return {
        "version": "1.0",
        "last_updated": datetime.now().strftime("%Y-%m-%d"),
        "apps": [generate_app(i + 1) for i in range(app_count)]
    }


# ----------------------------
# Interactive CLI
# ----------------------------

def main():
    print("=== Xbox App JSON Generator ===\n")

    while True:
        try:
            app_count = int(input("How many apps would you like to generate? "))
            if app_count <= 0:
                raise ValueError
            break
        except ValueError:
            print("Please enter a valid positive number.\n")

    output_file = input("Output filename (default: apps.json): ").strip()
    if not output_file:
        output_file = "apps.json"

    seed_input = input("Optional random seed (press Enter to skip): ").strip()
    if seed_input:
        random.seed(seed_input)
        print(f"Using seed: {seed_input}\n")

    data = generate_json(app_count)

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

    print(f"\nSuccessfully generated {app_count} apps.")
    print(f"Saved to: {output_file}")


if __name__ == "__main__":
    main()

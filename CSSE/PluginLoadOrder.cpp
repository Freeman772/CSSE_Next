#include "PluginLoadOrder.h"

namespace se::cs::plugins {
	std::string LoadOrder::key(const std::string& name) {
		auto result = name;
		for (auto& c : result) {
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c - 'A' + 'a');
			}
		}
		return result;
	}

	LoadOrder::LoadOrder(std::vector<PluginEntry> entries) : m_Entries(std::move(entries)) {
		m_Order.resize(m_Entries.size());
		std::iota(m_Order.begin(), m_Order.end(), size_t{0});
		std::stable_sort(m_Order.begin(), m_Order.end(), [this](size_t a, size_t b) {
			const auto& left = m_Entries[a];
			const auto& right = m_Entries[b];
			if (left.master != right.master) {
				return left.master;
			}
			if (left.pinned != right.pinned) {
				return left.pinned;
			}
			return left.originalTime < right.originalTime;
		});
		m_OriginalOrder = m_Order;
	}

	const std::vector<size_t>& LoadOrder::order() const { return m_Order; }
	const std::vector<PluginEntry>& LoadOrder::entries() const { return m_Entries; }
	void LoadOrder::setDependencies(size_t entry, std::vector<std::string> dependencies) {
		m_Entries.at(entry).dependencies = std::move(dependencies);
	}
	bool LoadOrder::changed() const { return m_Order != m_OriginalOrder; }
	void LoadOrder::reset() { m_Order = m_OriginalOrder; }
	void LoadOrder::acceptTimes(const std::vector<TimeChange>& changes) {
		for (const auto& change : changes) {
			m_Entries.at(change.entry).originalTime = change.after;
		}
		m_OriginalOrder = m_Order;
	}

	LoadOrder::ApplyResult LoadOrder::applyTimes(const std::vector<TimeChange>& changes, const std::function<bool(size_t, std::uint64_t)>& setter) {
		for (size_t i = 0; i < changes.size(); ++i) {
			if (!setter(i, changes[i].after)) {
				bool restored = true;
				for (size_t j = 0; j < changes.size(); ++j) {
					if (!setter(j, changes[j].before)) { restored = false; }
				}
				return restored ? ApplyResult::RolledBack : ApplyResult::RollbackFailed;
			}
		}
		return ApplyResult::Complete;
	}

	bool LoadOrder::validate(const std::vector<size_t>& order, std::string& error) const {
		std::unordered_map<std::string, size_t> positions;
		bool sawPlugin = false;
		bool sawUnpinnedMaster = false;
		for (size_t position = 0; position < order.size(); ++position) {
			const auto& entry = m_Entries[order[position]];
			if (!positions.emplace(key(entry.name), position).second) {
				error = "Duplicate plugin name: " + entry.name;
				return false;
			}
			if (entry.master && sawPlugin) {
				error = "Master files must remain before plugin files.";
				return false;
			}
			sawPlugin = sawPlugin || !entry.master;
			if (entry.master && entry.pinned && sawUnpinnedMaster) {
				error = "Morrowind, Tribunal and Bloodmoon must stay before other masters.";
				return false;
			}
			sawUnpinnedMaster = sawUnpinnedMaster || (entry.master && !entry.pinned);
		}
		for (size_t position = 0; position < order.size(); ++position) {
			const auto& entry = m_Entries[order[position]];
			for (const auto& dependency : entry.dependencies) {
				const auto found = positions.find(key(dependency));
				if (found != positions.end() && found->second >= position) {
					error = entry.name + " must load after " + dependency + ".";
					return false;
				}
			}
		}
		return true;
	}

	bool LoadOrder::move(const std::vector<size_t>& selected, std::optional<size_t> before, std::string& error) {
		error.clear();
		if (selected.empty()) {
			return true;
		}
		const std::unordered_set<size_t> selectedSet(selected.begin(), selected.end());
		for (const auto id : selectedSet) {
			if (id >= m_Entries.size()) {
				error = "The plugin list changed. Reopen the Data Files window.";
				return false;
			}
			if (m_Entries[id].pinned) {
				error = "The original game master files cannot be moved.";
				return false;
			}
		}
		if (before && selectedSet.contains(*before)) {
			return true;
		}
		std::vector<size_t> moved;
		std::vector<size_t> remaining;
		for (const auto id : m_Order) {
			(selectedSet.contains(id) ? moved : remaining).push_back(id);
		}
		auto insertion = before ? std::find(remaining.begin(), remaining.end(), *before) : remaining.end();
		if (before && insertion == remaining.end()) {
			error = "The drop target is no longer available.";
			return false;
		}
		remaining.insert(insertion, moved.begin(), moved.end());
		if (!validate(remaining, error)) {
			return false;
		}
		m_Order = std::move(remaining);
		return true;
	}

	std::vector<TimeChange> LoadOrder::planTimes() const {
		constexpr std::uint64_t step = 20'000'000; // Two seconds in Windows FILETIME ticks.
		std::vector<TimeChange> changes;
		if (!changed()) {
			return changes;
		}
		for (const bool master : {true, false}) {
			std::vector<std::uint64_t> slots;
			std::uint64_t previous = 0;
			for (const auto& entry : m_Entries) {
				if (entry.master != master) {
					continue;
				}
				if (entry.pinned) {
					previous = std::max(previous, entry.originalTime);
				}
				else {
					slots.push_back(entry.originalTime);
				}
			}
			std::sort(slots.begin(), slots.end());
			size_t slot = 0;
			for (const auto id : m_Order) {
				const auto& entry = m_Entries[id];
				if (entry.master != master || entry.pinned) {
					continue;
				}
				if (previous > std::uint64_t{0x7FFFFFFFFFFFFFFF} - step) {
					throw std::runtime_error("Plugin modification date is outside the supported FILETIME range.");
				}
				const auto target = std::max(slots.at(slot++), previous + step);
				if (target > std::uint64_t{0x7FFFFFFFFFFFFFFF}) {
					throw std::runtime_error("Plugin modification date is outside the supported FILETIME range.");
				}
				if (target != entry.originalTime) {
					changes.push_back({id, entry.originalTime, target});
				}
				previous = target;
			}
		}
		return changes;
	}

	std::vector<std::string> LoadOrder::readDependencies(std::istream& input) {
		const auto readWord = [&input]() {
			unsigned char bytes[4] = {};
			if (!input.read(reinterpret_cast<char*>(bytes), 4)) {
				throw std::runtime_error("Truncated TES3 header.");
			}
			return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8)
				| (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
		};
		char tag[4] = {};
		if (!input.read(tag, 4) || std::string_view(tag, 4) != "TES3") {
			throw std::runtime_error("The file does not contain a TES3 header.");
		}
		const auto size = readWord();
		readWord(); // Unknown field.
		readWord(); // Record flags.
		if (size > 16 * 1024 * 1024) {
			throw std::runtime_error("The TES3 header is too large.");
		}
		std::vector<std::string> dependencies;
		std::uint32_t remaining = size;
		while (remaining) {
			if (remaining < 8 || !input.read(tag, 4)) {
				throw std::runtime_error("Truncated TES3 subrecord.");
			}
			const auto length = readWord();
			remaining -= 8;
			if (length > remaining) {
				throw std::runtime_error("TES3 subrecord exceeds the header size.");
			}
			if (std::string_view(tag, 4) == "MAST") {
				if (length < 2 || length > 260) {
					throw std::runtime_error("Invalid TES3 master filename.");
				}
				std::string name(length, '\0');
				if (!input.read(name.data(), length) || name.back() != '\0') {
					throw std::runtime_error("Invalid TES3 master filename.");
				}
				name.pop_back();
				if (name.find_first_of("/\\:") != std::string::npos || name.find('\0') != std::string::npos) {
					throw std::runtime_error("Invalid TES3 master filename.");
				}
				dependencies.push_back(std::move(name));
			}
			else {
				// Read instead of seeking: seeking beyond EOF can succeed on an fstream.
				char buffer[4096];
				auto left = length;
				while (left) {
					const auto chunk = std::min(left, static_cast<std::uint32_t>(sizeof(buffer)));
					if (!input.read(buffer, chunk)) {
						throw std::runtime_error("Truncated TES3 header payload.");
					}
					left -= chunk;
				}
			}
			remaining -= length;
		}
		return dependencies;
	}
}

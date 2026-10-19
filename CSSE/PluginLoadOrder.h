#pragma once

namespace se::cs::plugins {

	struct PluginEntry {
		std::string name;
		bool master = false;
		bool pinned = false;
		std::uint64_t originalTime = 0;
		std::vector<std::string> dependencies;
	};

	struct TimeChange {
		size_t entry;
		std::uint64_t before;
		std::uint64_t after;
	};

	class LoadOrder {
	public:
		enum class ApplyResult { Complete, RolledBack, RollbackFailed };
		explicit LoadOrder(std::vector<PluginEntry> entries);
		const std::vector<size_t>& order() const;
		const std::vector<PluginEntry>& entries() const;
		void setDependencies(size_t entry, std::vector<std::string> dependencies);
		bool move(const std::vector<size_t>& selected, std::optional<size_t> before, std::string& error);
		bool changed() const;
		void reset();
		std::vector<TimeChange> planTimes() const;
		void acceptTimes(const std::vector<TimeChange>& changes);
		static ApplyResult applyTimes(const std::vector<TimeChange>& changes, const std::function<bool(size_t, std::uint64_t)>& setter);

		// Parse the TES3 header only. No plugin data or timestamps are modified.
		static std::vector<std::string> readDependencies(std::istream& input);

	private:
		bool validate(const std::vector<size_t>& order, std::string& error) const;
		static std::string key(const std::string& name);
		std::vector<PluginEntry> m_Entries;
		std::vector<size_t> m_Order;
		std::vector<size_t> m_OriginalOrder;
	};
}

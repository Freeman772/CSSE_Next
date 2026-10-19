#include "PluginLoadOrder.h"

using namespace se::cs::plugins;

void require(bool condition, const char* message) {
	if (!condition) { throw std::runtime_error(message); }
}

void word(std::string& out, std::uint32_t value) {
	for (int i = 0; i < 4; ++i) { out.push_back(static_cast<char>(value >> (8 * i))); }
}

int main() {
	constexpr std::uint64_t time = 130'000'000'000'000'000ULL;
	constexpr std::uint64_t step = 20'000'000;
	std::vector<PluginEntry> plugins = {
		{"Morrowind.esm", true, true, time, {}},
		{"Extra.esm", true, false, time + step, {"Morrowind.esm"}},
		{"Base.esp", false, false, time, {"Extra.esm"}},
		{"Patch.esp", false, false, time + step, {"BASE.ESP"}},
		{"Other.esp", false, false, time + step * 2, {"Morrowind.esm"}},
		{"Last.esp", false, false, time + step * 3, {}},
	};
	LoadOrder order(plugins);
	std::string error;
	require(order.move({4, 5}, 2, error), "Moving an independent group should work");
	require(order.order() == std::vector<size_t>({0, 1, 4, 5, 2, 3}), "Group order must be stable");
	require(!order.move({3}, 2, error), "A patch must not move before its dependency");
	require(!error.empty(), "Dependency failure must explain why");
	require(!order.move({2}, 1, error), "ESP must not move before ESM");
	require(!order.move({0}, std::nullopt, error), "Core masters must be pinned");
	const auto changes = order.planTimes();
	auto times = plugins;
	for (const auto& change : changes) { times[change.entry].originalTime = change.after; }
	require(times[0].originalTime == time, "Pinned master date must stay unchanged");
	require(times[4].originalTime < times[5].originalTime && times[5].originalTime < times[2].originalTime
		&& times[2].originalTime < times[3].originalTime, "Planned timestamps must encode the order");
	order.acceptTimes(changes);
	require(!order.changed() && order.planTimes().empty(), "A committed order must not be applied twice");
	require(order.move({5}, 4, error), "Second edit should work");
	order.reset();
	require(!order.changed(), "Reset should restore the last accepted order");
	LoadOrder duplicateTimes({{"a.esp", false, false, time, {}}, {"b.esp", false, false, time, {}}, {"c.esp", false, false, time, {}}});
	require(duplicateTimes.move({2}, 0, error), "Equal-date plugins should be movable");
	const auto planned = duplicateTimes.planTimes();
	require(planned.size() == 2, "Equal-date collisions must be resolved without rewriting every file");
	require(planned[0].after < planned[1].after, "Equal-date target slots must be distinct");
	std::vector<std::uint64_t> written;
	for (const auto& change : changes) { written.push_back(change.before); }
	bool failedOnce = false;
	const auto rollback = LoadOrder::applyTimes(changes, [&](size_t i, std::uint64_t value) {
		if (i == 1 && !failedOnce) { failedOnce = true; return false; }
		written[i] = value; return true;
	});
	require(rollback == LoadOrder::ApplyResult::RolledBack, "Partial write failure must roll back");
	for (size_t i = 0; i < changes.size(); ++i) { require(written[i] == changes[i].before, "Rollback must restore every original date"); }
	const auto failedRollback = LoadOrder::applyTimes(changes, [](size_t, std::uint64_t) { return false; });
	require(failedRollback == LoadOrder::ApplyResult::RollbackFailed, "Rollback failure must be distinguishable");
	std::string payload = "MAST";
	word(payload, 14); payload += std::string("Morrowind.esm\0", 14);
	payload += "DATA"; word(payload, 8); payload.append(8, '\0');
	std::string record = "TES3"; word(record, static_cast<std::uint32_t>(payload.size())); word(record, 0); word(record, 0); record += payload;
	std::istringstream input(record);
	require(LoadOrder::readDependencies(input) == std::vector<std::string>{"Morrowind.esm"}, "TES3 MAST parsing failed");
	for (const auto& bad : {record.substr(0, record.size() - 1), std::string("Not a TES3 file"), record.substr(0, 20)}) {
		bool rejected = false;
		try { std::istringstream stream(bad); LoadOrder::readDependencies(stream); }
		catch (const std::runtime_error&) { rejected = true; }
		require(rejected, "Truncated or malformed header must be rejected");
	}
	std::cout << "Plugin load-order tests passed: group moves, dependency guards, master boundaries, reset, timestamps, TES3 parsing.\n";
}

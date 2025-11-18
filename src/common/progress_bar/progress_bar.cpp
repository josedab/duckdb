#include "duckdb/common/progress_bar/progress_bar.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/common/progress_bar/display/terminal_progress_bar_display.hpp"

namespace duckdb {

void QueryProgress::Initialize() {
	percentage = -1;
	rows_processed = 0;
	total_rows_to_process = 0;
	elapsed_seconds = 0;
	estimated_remaining_seconds = -1;
	{
		lock_guard<mutex> guard(operator_lock);
		current_operator = "";
	}
	status = QueryProgressStatus::RUNNING;
}

void QueryProgress::Restart() {
	percentage = 0;
	rows_processed = 0;
	total_rows_to_process = 0;
	elapsed_seconds = 0;
	estimated_remaining_seconds = -1;
	{
		lock_guard<mutex> guard(operator_lock);
		current_operator = "";
	}
	status = QueryProgressStatus::RUNNING;
}

double QueryProgress::GetPercentage() {
	return percentage;
}
uint64_t QueryProgress::GetRowsProcesseed() {
	return rows_processed;
}
uint64_t QueryProgress::GetTotalRowsToProcess() {
	return total_rows_to_process;
}

double QueryProgress::GetElapsedSeconds() {
	return elapsed_seconds;
}

double QueryProgress::GetEstimatedRemainingSeconds() {
	return estimated_remaining_seconds;
}

string QueryProgress::GetCurrentOperator() {
	lock_guard<mutex> guard(operator_lock);
	return current_operator;
}

QueryProgressStatus QueryProgress::GetStatus() {
	return status;
}

void QueryProgress::SetElapsedSeconds(double elapsed) {
	elapsed_seconds = elapsed;
}

void QueryProgress::SetEstimatedRemainingSeconds(double remaining) {
	estimated_remaining_seconds = remaining;
}

void QueryProgress::SetCurrentOperator(const string &op) {
	lock_guard<mutex> guard(operator_lock);
	current_operator = op;
}

void QueryProgress::SetStatus(QueryProgressStatus new_status) {
	status = new_status;
}

QueryProgress::QueryProgress() {
	Initialize();
}

QueryProgress &QueryProgress::operator=(const QueryProgress &other) {
	if (this != &other) {
		percentage = other.percentage.load();
		rows_processed = other.rows_processed.load();
		total_rows_to_process = other.total_rows_to_process.load();
		elapsed_seconds = other.elapsed_seconds.load();
		estimated_remaining_seconds = other.estimated_remaining_seconds.load();
		{
			lock_guard<mutex> guard(operator_lock);
			lock_guard<mutex> other_guard(const_cast<mutex &>(other.operator_lock));
			current_operator = other.current_operator;
		}
		status = other.status.load();
	}
	return *this;
}

QueryProgress::QueryProgress(const QueryProgress &other) {
	percentage = other.percentage.load();
	rows_processed = other.rows_processed.load();
	total_rows_to_process = other.total_rows_to_process.load();
	elapsed_seconds = other.elapsed_seconds.load();
	estimated_remaining_seconds = other.estimated_remaining_seconds.load();
	{
		lock_guard<mutex> other_guard(const_cast<mutex &>(other.operator_lock));
		current_operator = other.current_operator;
	}
	status = other.status.load();
}

void ProgressBar::SystemOverrideCheck(ClientConfig &config) {
	if (config.system_progress_bar_disable_reason != nullptr) {
		throw InvalidInputException("Could not change the progress bar setting because: '%s'",
		                            config.system_progress_bar_disable_reason);
	}
}

unique_ptr<ProgressBarDisplay> ProgressBar::DefaultProgressBarDisplay() {
	return make_uniq<TerminalProgressBarDisplay>();
}

ProgressBar::ProgressBar(Executor &executor, idx_t show_progress_after,
                         progress_bar_display_create_func_t create_display_func)
    : executor(executor), show_progress_after(show_progress_after) {
	if (create_display_func) {
		display = create_display_func();
	}
}

QueryProgress ProgressBar::GetDetailedQueryProgress() {
	return query_progress;
}

void ProgressBar::Start() {
	profiler.Start();
	query_progress.Initialize();
	supported = true;
}

bool ProgressBar::PrintEnabled() const {
	return display != nullptr;
}

bool ProgressBar::ShouldPrint(bool final) const {
	if (!PrintEnabled()) {
		// Don't print progress at all
		return false;
	}
	if (!supported) {
		return false;
	}

	double elapsed_time = -1.0;
	if (elapsed_time < 0.0) {
		elapsed_time = profiler.Elapsed();
	}

	auto sufficient_time_elapsed = elapsed_time > static_cast<double>(show_progress_after) / 1000.0;
	if (!sufficient_time_elapsed) {
		// Don't print yet
		return false;
	}
	if (final) {
		// Print the last completed bar
		return true;
	}
	return query_progress.percentage > -1;
}

void ProgressBar::Update(bool final) {
	if (!final && !supported) {
		return;
	}

	ProgressData progress;
	idx_t invalid_pipelines = executor.GetPipelinesProgress(progress);

	double new_percentage = 0.0;
	if (invalid_pipelines == 0 && progress.IsValid()) {
		if (progress.total > 1e15) {
			progress.Normalize(1e15);
		}
		query_progress.rows_processed = idx_t(progress.done);
		query_progress.total_rows_to_process = idx_t(progress.total);
		new_percentage = progress.ProgressDone() * 100;
	}

	if (!final && invalid_pipelines > 0) {
		return;
	}

	if (new_percentage > query_progress.percentage) {
		query_progress.percentage = new_percentage;
	}

	// Update timing information
	double elapsed = profiler.Elapsed();
	query_progress.SetElapsedSeconds(elapsed);

	// Estimate remaining time based on progress
	double current_percentage = query_progress.percentage.load();
	if (current_percentage > 0 && current_percentage < 100) {
		double estimated_total = elapsed / (current_percentage / 100.0);
		double remaining = estimated_total - elapsed;
		query_progress.SetEstimatedRemainingSeconds(remaining > 0 ? remaining : 0);
	} else if (current_percentage >= 100) {
		query_progress.SetEstimatedRemainingSeconds(0);
	}

	// Update status
	if (final) {
		query_progress.SetStatus(QueryProgressStatus::FINISHED);
	} else {
		query_progress.SetStatus(QueryProgressStatus::RUNNING);
	}

	if (ShouldPrint(final)) {
		if (final) {
			FinishProgressBarPrint();
		} else {
			PrintProgress(query_progress.percentage.load());
		}
	}
}

void ProgressBar::PrintProgress(double current_percentage_p) {
	D_ASSERT(display);
	display->Update(current_percentage_p);
}

void ProgressBar::FinishProgressBarPrint() {
	if (finished) {
		return;
	}
	D_ASSERT(display);
	display->Finish();
	finished = true;
	if (query_progress.percentage == 0) {
		query_progress.Initialize();
	}
}

} // namespace duckdb

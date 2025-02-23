//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/persistent/base_csv_reader.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/parser/parsed_data/copy_info.hpp"
#include "duckdb/function/scalar/strftime_format.hpp"
#include "duckdb/common/types/chunk_collection.hpp"
#include "duckdb/common/enums/file_compression_type.hpp"
#include "duckdb/common/map.hpp"
#include "duckdb/common/queue.hpp"
#include "duckdb/execution/operator/persistent/csv_reader_options.hpp"
#include "duckdb/common/multi_file_reader.hpp"
#include "duckdb/execution/operator/persistent/csv_line_info.hpp"

#include <sstream>

namespace duckdb {
struct CopyInfo;
struct CSVFileHandle;
struct FileHandle;
struct StrpTimeFormat;

class FileOpener;
class FileSystem;

enum class ParserMode : uint8_t { PARSING = 0, SNIFFING_DIALECT = 1, SNIFFING_DATATYPES = 2, PARSING_HEADER = 3 };

//! Buffered CSV reader is a class that reads values from a stream and parses them as a CSV file
class BaseCSVReader {
public:
	BaseCSVReader(ClientContext &context, BufferedCSVReaderOptions options,
	              const vector<LogicalType> &requested_types = vector<LogicalType>());
	~BaseCSVReader();

	ClientContext &context;
	FileSystem &fs;
	Allocator &allocator;
	FileOpener *opener;
	BufferedCSVReaderOptions options;
	vector<LogicalType> return_types;
	vector<string> names;
	MultiFileReaderData reader_data;

	idx_t linenr = 0;
	bool linenr_estimated = false;

	bool row_empty = false;
	idx_t sample_chunk_idx = 0;
	bool jumping_samples = false;
	bool end_of_file_reached = false;
	bool bom_checked = false;

	idx_t bytes_in_chunk = 0;
	double bytes_per_line_avg = 0;

	DataChunk parse_chunk;

	std::queue<unique_ptr<DataChunk>> cached_chunks;

	ParserMode mode;

public:
	const string &GetFileName() {
		return options.file_path;
	}
	const vector<string> &GetNames() {
		return names;
	}
	const vector<LogicalType> &GetTypes() {
		return return_types;
	}
	virtual idx_t GetLineError(idx_t line_error, idx_t buffer_idx) {
		return line_error;
	};

	//! Initialize projection indices to select all columns
	void InitializeProjection();

protected:
	//! Initializes the parse_chunk with varchar columns and aligns info with new number of cols
	void InitParseChunk(idx_t num_cols);
	//! Change the date format for the type to the string
	void SetDateFormat(const string &format_specifier, const LogicalTypeId &sql_type);
	//! Try to cast a string value to the specified sql type
	bool TryCastValue(const Value &value, const LogicalType &sql_type);
	//! Try to cast a vector of values to the specified sql type
	bool TryCastVector(Vector &parse_chunk_col, idx_t size, const LogicalType &sql_type);

	//! Adds a value to the current row
	void AddValue(string_t str_val, idx_t &column, vector<idx_t> &escape_positions, bool has_quotes,
	              idx_t buffer_idx = 0);
	//! Adds a row to the insert_chunk, returns true if the chunk is filled as a result of this row being added
	bool AddRow(DataChunk &insert_chunk, idx_t &column, string &error_message, idx_t buffer_idx = 0);
	//! Finalizes a chunk, parsing all values that have been added so far and adding them to the insert_chunk
	bool Flush(DataChunk &insert_chunk, idx_t buffer_idx = 0, bool try_add_line = false);

	unique_ptr<CSVFileHandle> OpenCSV(const BufferedCSVReaderOptions &options);

	void VerifyUTF8(idx_t col_idx);
	void VerifyUTF8(idx_t col_idx, idx_t row_idx, DataChunk &chunk, int64_t offset = 0);
	string GetLineNumberStr(idx_t linenr, bool linenr_estimated, idx_t buffer_idx = 0);

	//! Sets the newline delimiter
	void SetNewLineDelimiter(bool carry = false, bool carry_followed_by_nl = false);

protected:
	//! Whether or not the current row's columns have overflown return_types.size()
	bool error_column_overflow = false;
	//! Number of sniffed columns - only used when auto-detecting
	vector<idx_t> sniffed_column_counts;
};

} // namespace duckdb

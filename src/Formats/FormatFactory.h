#pragma once

#include <Common/Allocator.h>
#include <Columns/IColumn.h>
#include <Formats/FormatSettings.h>
#include <Interpreters/Context_fwd.h>
#include <IO/BufferWithOwnMemory.h>
#include <IO/CompressionMethod.h>
#include <base/types.h>
#include <Core/NamesAndTypes.h>

#include <boost/noncopyable.hpp>

#include <functional>
#include <memory>
#include <unordered_map>


namespace DB
{

class Block;
struct Settings;
struct FormatFactorySettings;

class ReadBuffer;
class WriteBuffer;

class IProcessor;
using ProcessorPtr = std::shared_ptr<IProcessor>;

class IInputFormat;
class IOutputFormat;
class IRowOutputFormat;

struct RowInputFormatParams;

class ISchemaReader;
class IExternalSchemaReader;
using SchemaReaderPtr = std::shared_ptr<ISchemaReader>;
using ExternalSchemaReaderPtr = std::shared_ptr<IExternalSchemaReader>;

using InputFormatPtr = std::shared_ptr<IInputFormat>;
using OutputFormatPtr = std::shared_ptr<IOutputFormat>;
using RowOutputFormatPtr = std::shared_ptr<IRowOutputFormat>;

template <typename Allocator>
struct Memory;

FormatSettings getFormatSettings(ContextPtr context);

template <typename T>
FormatSettings getFormatSettings(ContextPtr context, const T & settings);

/** Allows to create an IInputFormat or IOutputFormat by the name of the format.
  * Note: format and compression are independent things.
  */
class FormatFactory final : private boost::noncopyable
{
public:
    /** Fast reading data from buffer and save result to memory.
      * Reads at least `min_bytes` and some more until the end of the chunk, depends on the format.
      * If `max_rows` is non-zero the function also stops after reading the `max_rows` number of rows
      * (even if the `min_bytes` boundary isn't reached yet).
      * Used in ParallelParsingInputFormat.
      */
    using FileSegmentationEngine = std::function<std::pair<bool, size_t>(
        ReadBuffer & buf,
        DB::Memory<Allocator<false>> & memory,
        size_t min_bytes,
        size_t max_rows)>;

private:
    using InputCreator = std::function<InputFormatPtr(
            ReadBuffer & buf,
            const Block & header,
            const RowInputFormatParams & params,
            const FormatSettings & settings)>;

    using OutputCreator = std::function<OutputFormatPtr(
            WriteBuffer & buf,
            const Block & sample,
            const FormatSettings & settings)>;

    /// Some input formats can have non trivial readPrefix() and readSuffix(),
    /// so in some cases there is no possibility to use parallel parsing.
    /// The checker should return true if parallel parsing should be disabled.
    using NonTrivialPrefixAndSuffixChecker = std::function<bool(ReadBuffer & buf)>;

    /// Some formats can support append depending on settings.
    /// The checker should return true if format support append.
    using AppendSupportChecker = std::function<bool(const FormatSettings & settings)>;

    using SchemaReaderCreator = std::function<SchemaReaderPtr(ReadBuffer & in, const FormatSettings & settings)>;
    using ExternalSchemaReaderCreator = std::function<ExternalSchemaReaderPtr(const FormatSettings & settings)>;

    /// Some formats can extract different schemas from the same source depending on
    /// some settings. To process this case in schema cache we should add some additional
    /// information to a cache key. This getter should return some string with information
    /// about such settings. For example, for Protobuf format it's the path to the schema
    /// and the name of the message.
    using AdditionalInfoForSchemaCacheGetter = std::function<String(const FormatSettings & settings)>;

    /// Some formats can support reading subset of columns depending on settings.
    /// The checker should return true if format support append.
    using SubsetOfColumnsSupportChecker = std::function<bool(const FormatSettings & settings)>;

    struct Creators
    {
        InputCreator input_creator;
        OutputCreator output_creator;
        FileSegmentationEngine file_segmentation_engine;
        SchemaReaderCreator schema_reader_creator;
        ExternalSchemaReaderCreator external_schema_reader_creator;
        bool supports_parallel_formatting{false};
        bool supports_subcolumns{false};
        NonTrivialPrefixAndSuffixChecker non_trivial_prefix_and_suffix_checker;
        AppendSupportChecker append_support_checker;
        AdditionalInfoForSchemaCacheGetter additional_info_for_schema_cache_getter;
        SubsetOfColumnsSupportChecker subset_of_columns_support_checker;
    };

    using FormatsDictionary = std::unordered_map<String, Creators>;
    using FileExtensionFormats = std::unordered_map<String, String>;

public:
    static FormatFactory & instance();

    InputFormatPtr getInput(
        const String & name,
        ReadBuffer & buf,
        const Block & sample,
        ContextPtr context,
        UInt64 max_block_size,
        const std::optional<FormatSettings> & format_settings = std::nullopt) const;

    InputFormatPtr getInputFormat(
        const String & name,
        ReadBuffer & buf,
        const Block & sample,
        ContextPtr context,
        UInt64 max_block_size,
        const std::optional<FormatSettings> & format_settings = std::nullopt) const;

    /// Checks all preconditions. Returns ordinary format if parallel formatting cannot be done.
    OutputFormatPtr getOutputFormatParallelIfPossible(
        const String & name,
        WriteBuffer & buf,
        const Block & sample,
        ContextPtr context,
        const std::optional<FormatSettings> & format_settings = std::nullopt) const;

    OutputFormatPtr getOutputFormat(
        const String & name,
        WriteBuffer & buf,
        const Block & sample,
        ContextPtr context,
        const std::optional<FormatSettings> & _format_settings = std::nullopt) const;

    String getContentType(
        const String & name,
        ContextPtr context,
        const std::optional<FormatSettings> & format_settings = std::nullopt) const;

    SchemaReaderPtr getSchemaReader(
        const String & name,
        ReadBuffer & buf,
        ContextPtr & context,
        const std::optional<FormatSettings> & format_settings = std::nullopt) const;

    ExternalSchemaReaderPtr getExternalSchemaReader(
        const String & name,
        ContextPtr & context,
        const std::optional<FormatSettings> & format_settings = std::nullopt) const;

    void registerFileSegmentationEngine(const String & name, FileSegmentationEngine file_segmentation_engine);

    void registerNonTrivialPrefixAndSuffixChecker(const String & name, NonTrivialPrefixAndSuffixChecker non_trivial_prefix_and_suffix_checker);

    void registerAppendSupportChecker(const String & name, AppendSupportChecker append_support_checker);

    /// If format always doesn't support append, you can use this method instead of
    /// registerAppendSupportChecker with append_support_checker that always returns true.
    void markFormatHasNoAppendSupport(const String & name);

    bool checkIfFormatSupportAppend(const String & name, ContextPtr context, const std::optional<FormatSettings> & format_settings_ = std::nullopt);

    /// Register format by its name.
    void registerInputFormat(const String & name, InputCreator input_creator);
    void registerOutputFormat(const String & name, OutputCreator output_creator);

    /// Register file extension for format
    void registerFileExtension(const String & extension, const String & format_name);
    String getFormatFromFileName(String file_name, bool throw_if_not_found = false);
    String getFormatFromFileDescriptor(int fd);

    /// Register schema readers for format its name.
    void registerSchemaReader(const String & name, SchemaReaderCreator schema_reader_creator);
    void registerExternalSchemaReader(const String & name, ExternalSchemaReaderCreator external_schema_reader_creator);

    void markOutputFormatSupportsParallelFormatting(const String & name);
    void markFormatSupportsSubcolumns(const String & name);

    bool checkIfFormatSupportsSubcolumns(const String & name) const;

    void markFormatSupportsSubsetOfColumns(const String & name);
    void registerSubsetOfColumnsSupportChecker(const String & name, SubsetOfColumnsSupportChecker subset_of_columns_support_checker);
    bool checkIfFormatSupportsSubsetOfColumns(const String & name, const ContextPtr & context, const std::optional<FormatSettings> & format_settings_ = std::nullopt) const;

    bool checkIfFormatHasSchemaReader(const String & name) const;
    bool checkIfFormatHasExternalSchemaReader(const String & name) const;
    bool checkIfFormatHasAnySchemaReader(const String & name) const;

    void registerAdditionalInfoForSchemaCacheGetter(const String & name, AdditionalInfoForSchemaCacheGetter additional_info_for_schema_cache_getter);
    String getAdditionalInfoForSchemaCache(const String & name, ContextPtr context, const std::optional<FormatSettings> & format_settings_ = std::nullopt);

    const FormatsDictionary & getAllFormats() const
    {
        return dict;
    }

    bool isInputFormat(const String & name) const;
    bool isOutputFormat(const String & name) const;

    /// Check that format with specified name exists and throw an exception otherwise.
    void checkFormatName(const String & name) const;

private:
    FormatsDictionary dict;
    FileExtensionFormats file_extension_formats;

    const Creators & getCreators(const String & name) const;

};

}

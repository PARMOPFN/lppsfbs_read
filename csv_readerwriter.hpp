
#ifndef SRC_SIMFEIF_UTILS_CSV_READER_WRITER_HPP_
#define SRC_SIMFEIF_UTILS_CSV_READER_WRITER_HPP_

#include <iterator>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <fstream>

namespace utils {

namespace detail { // for internal use only

// Write a single CSV value.
template <typename T>
void writeCsvIter(std::ostream& output_, bool prependComma, const T& val) {
    if (output_) {
        if (prependComma) {
            output_ << ", ";
        }

        output_ << val;
    }
}

// Write several CSV values.
template <typename T, typename ...RemArgs>
void writeCsvIter(std::ostream& output_, bool prependComma, const T& val, const RemArgs& ... args) {
    // Write the next value.
    writeCsvIter<T>(output_, prependComma, val);

    // Write recursively the remaining values.
    writeCsvIter<RemArgs...>(output_, true, args...);
}

} // end namespace detail



/* Line-oriented CSV writer supporting arbitrary column count and data types.
 *
 * Returns false in case of I/O error.
 *
 * Example usage:
 *
 *    std::ofstream file("myfile.csv");
 *
 *    writeCsvLine(file, 2.5, "dataA", 13);
 *    writeCsvLine(file, 4.1, "dataB", 5);
 *    writeCsvLine(file, 3.2, "dataC", 9);
 */
template <typename... Args>
bool writeCsvLine(std::ostream& output_, const Args& ... args) {
    detail::writeCsvIter<Args...>(output_, false, args...);
    if (output_) {
        output_ << "\n";
    }

    return (!output_.fail());
}



/*
 * CSV parser for fixed-column layout with mixed column data types.
 *
 * The parser accepts:
 *   - optional trailing commas at end of line,
 *   - optional empty lines,
 *
 * Example usage:
 *
 *    CsvReader<std::string, int> reader;
 *    std::ifstream file("myfile.csv");
 *
 *    if (!reader.parse(file)) {
 *        std::cout << "Error while reading line " << reader.errLine() << std::endl;
 *    } else {
 *       for (auto& line: reader.lines()) {
 *            std::string a;
 *            int b;
 *            std::tie(a, b) = line;
 *            std::cout << "first column element: " << a
 *                      << "; second column element: " << b
 *                      << std::endl;
 *            ... // Do something with a and b
 *        }
 *    }
 */
template <typename... Args>
class CsvReader {
    private:
        enum class Result { GOOD, BAD, EMPTY };

    public:
        using LineType = std::tuple<Args...>;
        using TableType = std::vector<LineType>;

    public:
        CsvReader() = default;

        bool parse(std::istream& input_) {
            _data.clear();

            // Loop over CSV data lines.
            _errLine = 0;
            while (true) {
                std::string line;
                LineType lineArgs;

                // Move to the next non-empty line.
                Result result;
                while (true) {
                    // Check if we are done.
                    if (input_.eof()) {
                        return (true);
                    } else if (input_.fail()) {
                        return (false);
                    }

                    // Attempt to read a line.
                    getline(input_, line);
                    ++_errLine;

                    // Parse the line and unpack into tuple.
                    std::istringstream lineStream(line);
                    result = parseIter<0, Args...>(lineStream, lineArgs);

                    if (result == Result::GOOD) {
                        break;
                    }
                }

                if(result == Result::GOOD) {
                    // Add the tuple to the table.
                    _data.emplace_back(std::move(lineArgs));
                }
            }
        }

        const TableType& lines() {
            return (_data);
        }

        std::size_t errLine() {
            return (_errLine);
        }

    private:


        // Check if a string contains only whitespaces.
        bool isEmpty(std::string& s) {
            for (char c: s) {
                if (!std::isspace(c))
                    return (false);
            }
            return (true);
        };

        // Check if a stream contains only lines with whitespaces
        // NOTE: the stream is consumed!
        bool isEmpty(std::istream& lineStream) {
            std::string nextToken;
            lineStream >> nextToken;

            return (isEmpty(nextToken));
        };


        template<std::size_t I, typename... RemArgs>
            inline typename std::enable_if<I == sizeof...(RemArgs), Result>::type
            parseIter(std::istream& lineStream_, std::tuple<RemArgs...>&) {
                return (isEmpty(lineStream_)? Result::GOOD : Result::BAD);
            }

        template<std::size_t I, typename... RemArgs>
            inline typename std::enable_if<I < sizeof...(RemArgs), Result>::type
            parseIter(std::istream& lineStream_, std::tuple<RemArgs...>& lineArgs_) {

                // Read next value as character string.
                std::string valueString;
                getline(lineStream_, valueString, ',');

                // For the first value in a line, an empty result is potentially OK and
                // may just signal an empty line.
                // However, a single comma without preceding value is not considered
                // an empty stream (it will be identified as invalid when attempting
                // to extract a value from an empty `valueString`).
                if (isEmpty(valueString)) {
                    if (I==0 && lineStream_.eof()) { // if not EOF, then a single trailing comma was found
                        return (Result::EMPTY);
                    } else {
                        return (Result::BAD);
                    }
                }

                // Extract the value as T.
                std::istringstream valueStream(valueString);
                valueStream >> std::get<I>(lineArgs_);

                if (valueStream.fail()) {
                    return (Result::BAD);
                }

                // The value stream should be fully consumed at this point.
                return (parseIter<I + 1, RemArgs...>(lineStream_, lineArgs_));
            }


    private:
        TableType _data;
        std::size_t _errLine;
};


template <typename Tkey, typename Tvalue>
   std::map <Tkey, Tvalue> load_and_parse_csv(std::string &filename) {
      std::map<Tkey, Tvalue> data;

       std::ifstream file(filename);
       if (!file) throw std::runtime_error("Load configuration from file: " +filename + " error: file not exist");

       utils::CsvReader<Tkey, Tvalue> reader;
       if (!reader.parse(file)) throw std::runtime_error("Load configuration from file: " + filename + " error: " + std::to_string(reader.errLine()));
       else {
           for (auto& line : reader.lines()) {
               std::string key;
               std::string value;
               std::tie(key, value) = line;
               //        std::cout << "load: [" << key << "] = (" << value <<")"<< std::endl;
               data[key] = value;
           } //for

       } //parseok
       return (data);
}

template <typename Tkey, typename Tvalue>
   void writeCsv(std::map<Tkey, Tvalue> data, std::string &filename) {

       std::ifstream file(filename);
       if (!file) throw std::runtime_error("Save configuration to file: " +filename + " error:");
       for (auto const x : data)
       {
           writeCsvLine(x.first, x.second );
       }

}


template<typename Tkey, typename Tval, typename Tsym>
void check_map_to_symbol(std::map<Tkey, Tval> data, std::string item, Tsym& symbol) {
    if ((data.find(item) != data.end()) && (symbol.stringToValue(data[item].c_str()))) ;
    else std::cerr << "Can't find [" << item << "] item, or wrong type of value. The default value (" << symbol << ") will be set" << std::endl;
}
/*
 * @brief convert value from map (probably string) to a value (probably numeric).
 * if need conversion error checking see strtod
 */
template<typename Tkey, typename Tval, typename T>
void check_map_to_value(std::map<Tkey, Tval> data, std::string item, T& val) {
    if (data.find(item) == data.end()) {
        std::cerr << "Can't find [" << item << "]";
        return;
    }
    std::stringstream sstr(data[item].c_str());
    sstr>>val;
}

}; //namespace utils
#endif

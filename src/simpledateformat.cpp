/*
 * Copyright 2003,2004 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log4cxx/helpers/simpledateformat.h>

#include <apr-1/apr_time.h>
#include <apr-1/apr_strings.h>

using namespace log4cxx;
using namespace log4cxx::helpers;

using namespace std;


SimpleDateFormat::PatternToken::PatternToken() {
}

SimpleDateFormat::PatternToken::~PatternToken() {
}

void SimpleDateFormat::PatternToken::setTimeZone(const TimeZonePtr& zone) {
}


void SimpleDateFormat::PatternToken::renderFacet(const std::locale& locale, 
							  std::ostream& buffer, 
							  const tm* time, 
							  const char spec) {
#if defined(_MSC_VER)
		_USE(locale, TimePutFacet).put(buffer, 
				 buffer, time, spec);
#else
		std::use_facet<TimePutFacet>(locale).put(buffer, 
				 buffer, buffer.fill(), time, spec);
#endif

}

namespace log4cxx {
  namespace helpers {
    namespace SimpleDateFormatImpl {

      class LiteralToken : public SimpleDateFormat::PatternToken {
      public:
        LiteralToken(char ch, int count) : ch(ch), count(count) {
        }
        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
          s.append(count, ch);
        }

      private:
        char ch;
        int count;
      };

      class EraToken : public SimpleDateFormat::PatternToken {
      public:
          EraToken(int count, const std::locale& locale)  {
          }
          void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
             s.append("AD");
          }
     };


      class NumericToken : public SimpleDateFormat::PatternToken {
      public:
        NumericToken(int width)
            : width(width) {
        }

        virtual int getField(const apr_time_exp_t& tm) const = 0;

        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
          char* fmt = apr_itoa(p, getField(tm));
          int len = strlen(fmt);
          if (width > len) {
            s.append(width - len, '0');
          }
          s.append(fmt);
        }

      private:
        int width;
        char zeroDigit;
      };

      class YearToken : public NumericToken {
      public:
        YearToken(int width) : NumericToken(width) {
        }

        int getField(const apr_time_exp_t& tm) const {
          return 1900 + tm.tm_year;
        }
      };

      class MonthToken : public NumericToken {
      public:
        MonthToken(int width) : NumericToken(width) {
        }

        int getField(const apr_time_exp_t& tm) const {
          return tm.tm_mon + 1;
        }
      };

      class AbbreviatedMonthNameToken : public SimpleDateFormat::PatternToken {
      public:
        AbbreviatedMonthNameToken(int width, const std::locale& locale) : names(12) {
          tm time;
          memset(&time, sizeof(time), 0);
          std::ostringstream buffer;
          size_t start = 0;
          for (int imon = 0; imon < 12; imon++) {
             time.tm_mon = imon;
			 renderFacet(locale, buffer, &time, 'b');
             std::string monthnames(buffer.str());
             names[imon] = monthnames.substr(start);
             start = monthnames.length();
          }
        }

        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
          s.append(names[tm.tm_mon]);
        }

      private:
        std::vector<std::string> names;

      };

      class FullMonthNameToken : public SimpleDateFormat::PatternToken {
      public:
        FullMonthNameToken(int width, const std::locale& locale) : names(12) {
          tm time;
          memset(&time, sizeof(time), 0);
          std::ostringstream buffer;
          size_t start = 0;
          for (int imon = 0; imon < 12; imon++) {
             time.tm_mon = imon;
             renderFacet(locale, buffer, &time, 'B');
             std::string monthnames(buffer.str());
             names[imon] = monthnames.substr(start);
             start = monthnames.length();
          }
        }

        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
          s.append(names[tm.tm_mon]);
        }

      private:
        std::vector<std::string> names;

      };

      class WeekInYearToken : public NumericToken {
      public:
          WeekInYearToken(int width) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
             return tm.tm_yday / 7;
          }
      };

      class WeekInMonthToken : public NumericToken {
      public:
          WeekInMonthToken(int width) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
             return tm.tm_mday / 7;
          }
      };


      class DayInMonthToken : public NumericToken {
      public:
          DayInMonthToken(int width) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
             return tm.tm_mday;
          }
      };

      class DayInYearToken : public NumericToken {
      public:
          DayInYearToken(int width) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
             return tm.tm_yday;
          }
      };


      class DayOfWeekInMonthToken : public NumericToken {
      public:
           DayOfWeekInMonthToken(int width) : NumericToken(width) {
           }

           int getField(const apr_time_exp_t& tm) const {
               return -1;
           }
      };

      class AbbreviatedDayNameToken : public SimpleDateFormat::PatternToken {
      public:
          AbbreviatedDayNameToken(int width, const std::locale& locale) : names(7) {
             tm time;
             memset(&time, sizeof(time), 0);
             std::ostringstream buffer;
             size_t start = 0;
             for (int iday = 0; iday < 7; iday++) {
                time.tm_wday = iday;
                renderFacet(locale, buffer, &time, 'a');
                std::string daynames(buffer.str());
                names[iday] = daynames.substr(start);
                start = daynames.length();
             }
          }

         void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
            s.append(names[tm.tm_wday]);
         }

        private:
            std::vector<std::string> names;

        };

        class FullDayNameToken : public SimpleDateFormat::PatternToken {
        public:
          FullDayNameToken(int width, const std::locale& locale) : names(7) {
            tm time;
            memset(&time, sizeof(time), 0);
            std::ostringstream buffer;
            size_t start = 0;
            for (int iday = 0; iday < 7; iday++) {
               time.tm_wday = iday;
               renderFacet(locale, buffer, &time, 'A');
               std::string daynames(buffer.str());
               names[iday] = daynames.substr(start);
               start = daynames.length();
            }
          }

          void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
            s.append(names[tm.tm_wday]);
          }

        private:
          std::vector<std::string> names;

        };


      class MilitaryHourToken : public NumericToken {
          public:
          MilitaryHourToken(int width, int offset) :
             NumericToken(width), offset(offset) {
          }

          int getField(const apr_time_exp_t& tm) const {
               return tm.tm_hour + offset;
          }

          private:
          int offset;
      };

      class HourToken : public NumericToken {
      public:
          HourToken(int width, int offset) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
              return ((tm.tm_hour + 12 - offset) % 12) + offset;
          }

          private:
          int offset;
      };

     class MinuteToken : public NumericToken {
     public:
          MinuteToken(int width) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
             return tm.tm_min;
          }
    };

    class SecondToken : public NumericToken {
    public:
         SecondToken(int width) : NumericToken(width) {
         }

         int getField(const apr_time_exp_t& tm) const {
            return tm.tm_sec;
         }
    };

    class MillisecondToken : public NumericToken {
    public:
          MillisecondToken(int width) : NumericToken(width) {
          }

          int getField(const apr_time_exp_t& tm) const {
             return tm.tm_usec / 1000;
          }
    };

    class AMPMToken : public SimpleDateFormat::PatternToken  {
    public:
        AMPMToken(int width, const std::locale& locale) : names(2)  {
          tm time;
          memset(&time, sizeof(time), 0);
          std::ostringstream buffer;
          size_t start = 0;
          for (int i = 0; i < 2; i++) {
             time.tm_hour = i * 12;
             renderFacet(locale, buffer, &time, 'p');
             std::string ampm = buffer.str();
             names[i] = ampm.substr(start);
             start = ampm.length();
          }
        }

        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
           s.append(names[tm.tm_hour / 12]);
        }

        private:
        std::vector<std::string> names;
    };


    class GeneralTimeZoneToken : public SimpleDateFormat::PatternToken  {
    public:
        GeneralTimeZoneToken(int width)  {
        }

        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
           s.append(timeZone->getID());
        }

        void setTimeZone(const TimeZonePtr& zone) {
          timeZone = zone;
        }

        private:
        TimeZonePtr timeZone;
    };

    class RFC822TimeZoneToken : public SimpleDateFormat::PatternToken  {
    public:
        RFC822TimeZoneToken(int width)  {
        }

        void format(std::string& s, const apr_time_exp_t& tm, apr_pool_t* p) const {
           if (tm.tm_gmtoff == 0) {
             s.append(1, 'Z');
           } else {
             apr_int32_t off = tm.tm_gmtoff;
             if (off > 0) {
               s.append(1, '+');
             } else {
               s.append(1, '-');
               off = -off;
             }
             char* hours = apr_itoa(p, off/3600);
             if (hours[1] == 0) {
               s.append(1, '0');
             }
             s.append(hours);
             char* min = apr_itoa(p, (off % 3600) / 60);
             if (min[1] == 0) {
               s.append(1, '0');
             }
             s.append(min);
           }
        }
    };


    }
  }
}


SimpleDateFormat::SimpleDateFormat(const String& fmt)
  : timeZone(TimeZone::getDefault()) {
  std::locale defaultLocale;
  parsePattern(fmt, defaultLocale, pattern);
  for(PatternTokenList::iterator iter = pattern.begin();
      iter != pattern.end();
      iter++) {
      (*iter)->setTimeZone(timeZone);
  }
}

SimpleDateFormat::SimpleDateFormat(const String& fmt, const std::locale& locale)
  : timeZone(TimeZone::getDefault()) {
    parsePattern(fmt, locale, pattern);
    for(PatternTokenList::iterator iter = pattern.begin();
        iter != pattern.end();
        iter++) {
        (*iter)->setTimeZone(timeZone);
    }
}


SimpleDateFormat::~SimpleDateFormat() {
  for(PatternTokenList::iterator iter = pattern.begin();
      iter != pattern.end();
      iter++) {
      delete *iter;
  }
}

void SimpleDateFormat::addToken(const char spec,
                                const int repeat,
                                const std::locale& locale,
                                PatternTokenList& pattern) {
    switch(spec) {
      case 'G':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::EraToken(repeat, locale));
      break;

      case 'y':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::YearToken(repeat));
      break;

      case 'M':
      if (repeat <= 2) {
         pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::MonthToken(repeat));
      } else if (repeat <= 3) {
        pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::AbbreviatedMonthNameToken(repeat, locale));
      } else {
        pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::FullMonthNameToken(repeat, locale));
      }
      break;

      case 'w':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::WeekInYearToken(repeat));
      break;

      case 'W':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::WeekInMonthToken(repeat));
      break;

      case 'D':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::DayInYearToken(repeat));
      break;

      case 'd':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::DayInMonthToken(repeat));
      break;

      case 'F':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::DayOfWeekInMonthToken(repeat));
      break;

      case 'E':
      if (repeat <= 3) {
        pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::AbbreviatedDayNameToken(repeat, locale));
      } else {
        pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::FullDayNameToken(repeat, locale));
      }
      break;

      case 'a':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::AMPMToken(repeat, locale));
      break;

      case 'H':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::MilitaryHourToken(repeat, 0));
      break;

      case 'k':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::MilitaryHourToken(repeat, 1));
      break;

      case 'K':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::HourToken(repeat, 0));
      break;

      case 'h':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::HourToken(repeat, 1));
      break;

      case 'm':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::MinuteToken(repeat));
      break;

      case 's':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::SecondToken(repeat));
      break;

      case 'S':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::MillisecondToken(repeat));
      break;

      case 'z':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::GeneralTimeZoneToken(repeat));
      break;

      case 'Z':
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::RFC822TimeZoneToken(repeat));
      break;

      default:
      pattern.push_back(new log4cxx::helpers::SimpleDateFormatImpl::LiteralToken(spec, repeat));
    }
}

void SimpleDateFormat::parsePattern(const String& fmt,
      const std::locale& locale,
      PatternTokenList& pattern) {
     if (!fmt.empty()) {
        String::const_iterator iter = fmt.begin();
        int repeat = 1;
        char prevChar = *iter;
        for(iter++; iter != fmt.end(); iter++) {
          if (*iter == prevChar) {
            repeat++;
          } else {
            addToken(prevChar, repeat, locale, pattern);
            prevChar = *iter;
            repeat = 1;
          }
        }
        addToken(prevChar, repeat, locale, pattern);
     }
}

void SimpleDateFormat::format(std::string& s, apr_time_t time, apr_pool_t* p) const  {
  apr_time_exp_t exploded;
  apr_status_t stat = timeZone->explode(&exploded, time);
  if (stat == APR_SUCCESS) {
    for(PatternTokenList::const_iterator iter = pattern.begin();
        iter != pattern.end();
        iter++) {
        (*iter)->format(s, exploded, p);
    }
  }
}

void SimpleDateFormat::setTimeZone(const TimeZonePtr& zone) {
  timeZone = zone;
}


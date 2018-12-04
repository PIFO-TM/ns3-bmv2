/*
Copyright 2018 Stanford University
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

typedef bit<32> uint_t;
/*
 * The following bitwidth is a parameter that can be adjusted to make
 * the division operation more or less accurate.
 */
typedef bit<10> log_uint_t;

/*
 * Here we will use lookup tables to approximate integer division.
 * We will use the following useful fact:
 *     A/B = exp(log(A) - log(B))
 * We will use ternary tables to approximate log() and exact match
 * tables to implement exp()
 * See this paper for more details:
 *     https://homes.cs.washington.edu/~arvind/papers/flexswitch.pdf
 */
control divide_pipe(in uint_t numerator,
                    in uint_t denominator,
                    out uint_t result) {

    log_uint_t log_num;
    log_uint_t log_denom;
    log_uint_t log_result;

    action set_log_num(log_uint_t val) {
        log_num = val;
    }

    table log_numerator {
        key = { numerator: ternary; }
        actions = { set_log_num; }
        size = 1024;
        default_action = set_log_num(0);
    }

    action set_log_denom(log_uint_t val) {
        log_denom = val;
    }

    table log_denominator {
        key = { denominator: ternary; }
        actions = { set_log_denom; }
        size = 1024;
        default_action = set_log_denom(0);
    }

    action set_result(uint_t val) {
        result = val;
    }

    table exp {
        key = { log_result: exact; }
        actions = { set_result; }
        size = 2048;
        default_action = set_result(0);
    }

    apply {
        // numerator / denominator = exp(log(numerator) - log(denominator))
        if (numerator == 0 || denominator == 0 || denominator > numerator) {
            result = 0;
        } else {
            log_numerator.apply();
            log_denominator.apply();
            log_result = log_num - log_denom;
            exp.apply();
        }
    }
}


module MbuniBenchmarking
  module TestModule
    
    def assert(assertion, description)
      results_so_far = get_result(@running_benchmark, :test_assertions)
      results_so_far ||= []
      results_so_far << [description, assertion ? "OK" : "Failed" ]
      add_result(:test_assertions, results_so_far)
    end
    
    def add_assertion_result(description, result)
      results_so_far = get_result(@running_benchmark, :test_assertions)
      results_so_far ||= []
      results_so_far << [description, result]
      add_result(:test_assertions, results_so_far)      
    end

    def test_module_create_report(key)
      test_results = get_result(key, :test_assertions)
      return if test_results.nil?
      test_report = "<table border=1>\n" + "<th colspan='2' align='left'>Test module results for #{key.to_s}:</th><tr/>\n"
      test_results.each do |test_description, test_result|
        test_report += "<td align='left'>#{test_description}</td><td align='right'>#{test_result}</td><tr/>\n"
      end
      test_report += "</table>"
      
      @report[key].add_details test_report
    end
  end
  
  def assertion(description)
    begin 
      result = (yield) ? "OK" : "Failed"
    rescue Exception => test_exception
      result = test_exception.to_s
    end
    add_assertion_result(description,result)
  end
end
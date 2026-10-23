Gem::Specification.new do |s|
  s.name = 'openc3-cosmos-mkr-rtd'
  s.version = '0.3.0'
  s.summary = 'MKR RTD temperature, UART oscilloscope and servo control'
  s.description = 'Adds servo command and telemetry while preserving RTD, LED, period and UART features'
  s.license = 'MIT'
  s.authors = ['OpenC3 community example adaptation']
  s.homepage = 'https://github.com/OpenC3/openc3-cosmos-arduino-sensor-kit'
  s.metadata = { 'openc3_minimum_cosmos_version' => '5.0.0' }
  s.files = Dir.glob('targets/**/*').select { |f| File.file?(f) } + %w[plugin.txt README.md]
end

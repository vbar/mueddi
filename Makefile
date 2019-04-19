.PHONY: setup
setup:
	python3 make_source.py struct_hash dawg leven

# doesn't work in virtual environment - not clear why...
.PHONY: dist
dist:
	python3 setup.py sdist bdist_wheel

upload:
	twine upload dist/*

.PHONY: test
test:
	python3 -m unittest mueddit.tests.test

# needs root access to install outside virtual environment
.PHONY: install
install:
	python3 setup.py install

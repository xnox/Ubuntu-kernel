
reports: fetch-data sru-report.html versions.html

fetch-data:
	@/home/bradf/bin/kteam/stable/sru-report --archive-versions > sru-data-tmp.json
	@if [ -s sru-data-tmp.json ]; then \
	    mv sru-data-tmp.json sru-data.json; \
	fi

sru-report.html: sru-data.json scripts/sru2html
	scripts/sru2html $< > $@

versions.html: sru-data.json scripts/versions2html
	scripts/versions2html $< > $@

clean:
	rm -f *.json sru-report.html versions.html

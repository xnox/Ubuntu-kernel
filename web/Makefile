
reports: fetch-data sru-report.html versions.html

fetch-data:
	@kteam-tools/stable/sru-report --archive-versions > sru-data-tmp.json
	@if [ -s sru-data-tmp.json ]; then \
	    mv sru-data-tmp.json sru-data.json; \
	fi

sru-report.html: sru-data.json scripts/sru2html
	scripts/sru2html $< > $@

versions.html: sru-data.json scripts/versions2html
	scripts/versions2html $< > $@



regressions-data: regressions-potential-data.json regressions-release-data.json regressions-update-data.json

.PHONY : regressions-potential-data.json
regressions-potential-data.json: scripts/collect-regressions-data
	scripts/collect-regressions-data --potential --db=$@

.PHONY : regressions-release-data.json
regressions-release-data.json: scripts/collect-regressions-data
	scripts/collect-regressions-data --release --db=$@

.PHONY : regressions-update-data.json
regressions-update-data.json: scripts/collect-regressions-data
	scripts/collect-regressions-data --update --db=$@


regressions-reports: regressions-potential-report.html regressions-release-report.html regressions-updates-report.html

regressions-potential-report.html: scripts/mk-regressions-report
	scripts/mk-regressions-report --potential > $@

regressions-release-report.html: scripts/mk-regressions-report
	scripts/mk-regressions-report --release > $@

regressions-updates-report.html: scripts/mk-regressions-report
	scripts/mk-regressions-report --updates > $@

clean:
	rm -f *.json sru-report.html versions.html

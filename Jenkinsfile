// Runs in the home lab's Jenkins: the Linux stages of scripts/test.sh, one pod
// each and in parallel, the way the Linux jobs in .github/workflows/ci.yml run.
// Each pod runs the blogin-dev image built from docker/, so a stage that would
// start that container runs its commands directly instead (BLOGIN_IN_CONTAINER).
// The macOS stages and fuzzing stay on GitHub, which starts once this passes and
// Jenkins reports the continuous-integration/jenkins/branch status.

// A function rather than a variable: a `def` at the top of a Jenkinsfile is
// local to the script body, and linuxStage() below cannot see it.
def stagePod() {
  return '''
  apiVersion: v1
  kind: Pod
  spec:
    containers:
      - name: dev
        image: harbor.home.gregdonald.com/blogin/dev:latest
        imagePullPolicy: Always
        command: ["sleep"]
        args: ["infinity"]
        env:
          - name: BLOGIN_IN_CONTAINER
            value: "1"
          - name: BLOGIN_JOBS
            value: "4"
        resources:
          requests:
            cpu: "2"
            memory: 4Gi
          limits:
            cpu: "4"
            memory: 6Gi
'''
}

def linuxStage(String name) {
  return {
    stage(name) {
      podTemplate(yaml: stagePod()) {
        node(POD_LABEL) {
          container('dev') {
            checkout scm
            try {
              sh "./scripts/test.sh -j 4 ${name}"
            } finally {
              archiveArtifacts artifacts: "build/test-logs/${name}.log", allowEmptyArchive: true
            }
          }
        }
      }
    }
  }
}

pipeline {
  agent none

  options {
    disableConcurrentBuilds(abortPrevious: true)
  }

  stages {
    stage('linux stages') {
      steps {
        script {
          def names = ['linux', 'linux-static', 'gcc', 'tidy', 'valgrind', 'coverage-linux']
          parallel names.collectEntries { [(it): linuxStage(it)] }
        }
      }
    }
  }
}
